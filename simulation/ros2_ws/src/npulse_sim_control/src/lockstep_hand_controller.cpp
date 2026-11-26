// Simulation-only scheduling adapter. Production C executes inside the physics update.
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <memory>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <mutex>
#include <string>
#include <vector>
#include "controller_interface/controller_interface.hpp"
#include "pluginlib/class_list_macros.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_lifecycle/lifecycle_publisher.hpp"
#include "npulse_sim_control/msg/hand_command.hpp"
#include "npulse_sim_control/msg/hand_state.hpp"
#include "mechanics.hpp"
#include "hand_firmware.h"
namespace npulse_sim_control {

// Finite functional test. This scheduler issues commands, never motor outputs.
// Production C still computes every output and retains its safety/guard checks.
struct HandTestSequence {
 struct Stage {const char *name;int action;};
 static constexpr std::array<Stage,19> stages={{{"HOLD_CURRENT",-3},
  {"INDEX_OPEN",7},{"INDEX_CLOSE",8},{"MIDDLE_OPEN",9},{"MIDDLE_CLOSE",10},
  {"RING_OPEN",11},{"RING_CLOSE",12},{"LITTLE_OPEN",13},{"LITTLE_CLOSE",14},
  {"THUMB_OPEN",5},{"THUMB_CLOSE",6},{"OPEN_HAND",0},{"CLOSE_HAND",1},
  {"PINCH",2},{"WRIST_PALM",15},{"WRIST_DORSI",16},
  {"ROTATE_WRIST_R",3},{"ROTATE_WRIST_L",4},{"NEUTRAL",-2}}};
 bool started=false,finished=false,failed=false,timing=false,settling=false;
 size_t stage=0;
 uint32_t global_start=0,stage_start=0,settled_since=0;
 std::string event;
 size_t index()const{return std::min(stage,stages.size()-1);}
 uint64_t revision()const{return started?index()+1:0;}
 void fail(const std::string &why){finished=failed=true;event="AUTO_TEST FAIL "+std::string(stages[index()].name)+": "+why;}
 bool tick(uint32_t ms,bool start,bool stop){
  if(stop && !finished){fail("operator STOP");return false;}
  if(start && !started && !finished){started=true;global_start=ms;event="AUTO_TEST START: HOLD_CURRENT; all eight motors";}
  if(started && !finished && (uint32_t)(ms-global_start)>300000U)fail("300 simulated second test deadline");
  return started&&!finished;
 }
 void observe(uint32_t ms,const npulse_hand_result &r,double shape_error){
  if(!started||finished)return;
  if(r.phase==3){fail("firmware fault causes="+std::to_string(r.causes));return;}
  if(r.phase!=2){
   if((uint32_t)(ms-global_start)>2000U && stage==0)fail("firmware did not arm in 2 simulated seconds");
   return;
  }
  if(!r.armed||r.enabled_mask!=255||r.causes){fail("firmware not fully enabled");return;}
  if(!timing){stage_start=ms;timing=true;settling=false;}
  double error=0,speed=0;
  for(size_t i=0;i<8;++i){error=std::max(error,std::abs(r.target[i]-r.position[i]));speed=std::max(speed,std::abs(r.velocity[i]));}
  bool good=error<0.03490658504 && speed<0.05 && shape_error<0.03490658504;
  if(good){if(!settling){settling=true;settled_since=ms;}}
  else settling=false;
  uint32_t minimum=stage==0?2000U:1000U;
  if(good && settling && (uint32_t)(ms-settled_since)>=500U && (uint32_t)(ms-stage_start)>=minimum){
   event="AUTO_TEST PASS "+std::string(stages[index()].name)+" max_error_deg="+std::to_string(error*57.295779513);
   ++stage;timing=settling=false;
   if(stage==stages.size()){finished=true;event+="; COMPLETE — STOP requested";}
   else event+="; NEXT "+std::string(stages[index()].name);
  }else if((uint32_t)(ms-stage_start)>12000U){
   fail("no convergence in 12 simulated seconds; max_error_deg="+std::to_string(error*57.295779513)+
        " speed_rad_s="+std::to_string(speed)+" shape_error_deg="+std::to_string(shape_error*57.295779513));
  }
 }
};

class LockstepHandController : public controller_interface::ControllerInterface {
 using Cmd=msg::HandCommand;using State=msg::HandState;
 std::vector<std::string> joints_;
 std::array<size_t,17> pi_{},vi_{},ci_{};
 std::array<double,8> scales_{};
 std::array<double,17> limits_{};
 double coupling_k_=0.3,coupling_d_=0.01;
 bool preserve_others_=true;
 bool automatic_test_=false;
 uint8_t automatic_limit_=15;
 uint64_t automatic_session_=0,trace_last_wall_=0;
 std::atomic<bool> test_start_{false},test_stop_{false};
 HandTestSequence test_;
 std::ofstream trace_;
 int trace_state_outcome_=0;
 static constexpr std::array<const char *,8> motor_names_={"thumb","index","middle","ring","little","wrist_x","wrist_y","palm"};
 void write_test_trace(const State &s,int outcome){
  if(!trace_.is_open()||trace_last_wall_==s.wall_ns)return;
  trace_last_wall_=s.wall_ns;
  const char *label=s.revision>=1&&s.revision<=HandTestSequence::stages.size()?HandTestSequence::stages[s.revision-1].name:"IDLE";
  trace_<<s.wall_ns<<','<<s.sim_ms<<','<<s.firmware_run<<','<<(unsigned)s.phase<<','<<s.causes<<','<<s.revision<<','<<label<<','<<s.preserve_others<<','<<outcome;
  for(size_t i=0;i<8;++i)trace_<<','<<s.target[i]<<','<<s.reference[i]<<','<<s.position[i]<<','<<s.velocity[i]
    <<','<<s.requested_percent[i]<<','<<s.applied_percent[i]<<','<<s.feedback_age_ms[i]<<','<<s.kp[i]<<','<<s.ki[i]<<','<<s.kd[i];
  for(double value:s.joint_position)trace_<<','<<value;
  for(double value:s.joint_velocity)trace_<<','<<value;
  for(double value:s.joint_effort)trace_<<','<<value;
  trace_<<'\n';trace_.flush();
 }
 std::mutex input_mutex_,state_mutex_;
 Cmd input_,cached_;
 State state_;
 bool received_=false,have_cached_=false,have_state_=false;
 std::atomic<bool> active_{false};
 Lease lease_;
 bool was_allowed_=false;
 uint64_t timeout_ns_=500000000;
 std::array<double,8> applied_{};
 std::array<double,17> coupling_rest_{};
 bool have_coupling_rest_=false;
 uint64_t coupling_session_=0,coupling_revision_=0;
 uint32_t coupling_run_=0;
 rclcpp::Subscription<Cmd>::SharedPtr sub_;
 rclcpp_lifecycle::LifecyclePublisher<State>::SharedPtr pub_;
 rclcpp::TimerBase::SharedPtr timer_;
 static uint64_t wall_ns(){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
public:
 controller_interface::CallbackReturn on_init() override {
  auto_declare<std::vector<std::string>>("joints",{});
  auto_declare<std::vector<double>>("nm_per_percent",{0.005,0.005,0.005,0.005,0.005,0.01,0.01,0.005});
  auto_declare<std::vector<double>>("joint_limits",{});
  auto_declare<double>("coupling_stiffness",0.3);
  auto_declare<double>("coupling_damping",0.01);
  auto_declare<bool>("preserve_others",true);
  auto_declare<bool>("automatic_test",false);
  auto_declare<int64_t>("automatic_test_limit",15);
  auto_declare<std::string>("test_csv_directory","/tmp");
  return controller_interface::CallbackReturn::SUCCESS;
 }
 controller_interface::InterfaceConfiguration command_interface_configuration() const override {
  controller_interface::InterfaceConfiguration result;
  result.type=controller_interface::interface_configuration_type::INDIVIDUAL;
  for(const auto &j:joints_)result.names.push_back(j+"/effort");
  return result;
 }
 controller_interface::InterfaceConfiguration state_interface_configuration() const override {
  controller_interface::InterfaceConfiguration result;
  result.type=controller_interface::interface_configuration_type::INDIVIDUAL;
  for(const auto & j:joints_){result.names.push_back(j+"/position");result.names.push_back(j+"/velocity");}
  return result;
 }
 controller_interface::CallbackReturn on_configure(const rclcpp_lifecycle::State &) override {
  joints_=get_node()->get_parameter("joints").as_string_array();
  auto scales=get_node()->get_parameter("nm_per_percent").as_double_array();
  auto limits=get_node()->get_parameter("joint_limits").as_double_array();
  const std::vector<std::string> expected={"joint_thumb_2","joint_thumb_3","joint_index_1","joint_index_2","joint_index_3",
   "joint_middle_1","joint_middle_2","joint_middle_3","joint_ring_1","joint_ring_2","joint_ring_3",
   "joint_little_1","joint_little_2","joint_little_3","joint_wrist_x","joint_wrist_y","joint_thumb_1_x"};
  if(joints_!=expected || scales.size()!=8 || limits.size()!=17)return controller_interface::CallbackReturn::ERROR;
  for(size_t i=0;i<8;++i){if(!std::isfinite(scales[i])||scales[i]<=0)return controller_interface::CallbackReturn::ERROR;scales_[i]=scales[i];}
  for(size_t i=0;i<17;++i){if(!std::isfinite(limits[i])||limits[i]<=0)return controller_interface::CallbackReturn::ERROR;limits_[i]=limits[i];}
  coupling_k_=get_node()->get_parameter("coupling_stiffness").as_double();
  coupling_d_=get_node()->get_parameter("coupling_damping").as_double();
  preserve_others_=get_node()->get_parameter("preserve_others").as_bool();
  if(!std::isfinite(coupling_k_)||coupling_k_<0||!std::isfinite(coupling_d_)||coupling_d_<0)return controller_interface::CallbackReturn::ERROR;
  automatic_test_=get_node()->get_parameter("automatic_test").as_bool();
  auto test_limit=get_node()->get_parameter("automatic_test_limit").as_int();
  if(test_limit<1||test_limit>100)return controller_interface::CallbackReturn::ERROR;
  automatic_limit_=static_cast<uint8_t>(test_limit);
  if(automatic_test_){
   try{
    auto directory=std::filesystem::path(get_node()->get_parameter("test_csv_directory").as_string());
    std::filesystem::create_directories(directory);
    auto path=directory/("npulse-auto-test-"+std::to_string(wall_ns())+".csv");
    trace_.open(path);if(!trace_)return controller_interface::CallbackReturn::ERROR;
    trace_<<std::setprecision(17)<<"wall_ns,sim_ms,run,phase,causes,revision,command,preserve_others,test_outcome";
    for(const char *motor:motor_names_)for(const char *field:{"target","reference","position","velocity","requested_percent","applied_percent","feedback_age_ms","kp","ki","kd"})trace_<<','<<motor<<'_'<<field;
    for(const char *field:{"position","velocity","effort"})for(int i=0;i<17;++i)trace_<<",joint_"<<i<<'_'<<field;
    trace_<<'\n';trace_.flush();
    RCLCPP_INFO(get_node()->get_logger(),"AUTO_TEST ready; CSV=%s; waiting for one explicit enabled=true command",path.string().c_str());
   }catch(const std::exception &e){RCLCPP_ERROR(get_node()->get_logger(),"AUTO_TEST CSV error: %s",e.what());return controller_interface::CallbackReturn::ERROR;}
  }
  sub_=get_node()->create_subscription<Cmd>("~/hand_command",rclcpp::QoS(1),[this](const Cmd::SharedPtr msg){
   if(automatic_test_){
    if(active_){if(msg->enabled)test_start_=true;else test_stop_=true;}
    return;
   }
   if(!active_ || !msg->session || !msg->limit || msg->limit>100 || msg->action < -3 || msg->action > 16)return;
   if(msg->action==-1 && !msg->target_mask)return;
   for(double q:msg->targets)if(!std::isfinite(q)||std::abs(q)>2)return;
   auto now=wall_ns();if(msg->wall_ns>now || now-msg->wall_ns>timeout_ns_)return;
   std::lock_guard<std::mutex> lock(input_mutex_);
   if(received_ && (msg->session<input_.session || (msg->session==input_.session&&msg->sequence<=input_.sequence)))return;
   input_=*msg;received_=true;
  });
  pub_=get_node()->create_publisher<State>("~/hand_state",rclcpp::QoS(1));
  timer_=get_node()->create_wall_timer(std::chrono::milliseconds(20),[this](){
   if(!active_)return;
   State copy;int outcome;{std::lock_guard<std::mutex> lock(state_mutex_);if(!have_state_)return;copy=state_;outcome=trace_state_outcome_;}
   pub_->publish(copy); // Telemetry cannot delay the C feedback/control path.
   if(automatic_test_)write_test_trace(copy,outcome);
  });
  return controller_interface::CallbackReturn::SUCCESS;
 }
 controller_interface::CallbackReturn on_activate(const rclcpp_lifecycle::State &) override {
  if(command_interfaces_.size()!=17||state_interfaces_.size()!=34)return controller_interface::CallbackReturn::ERROR;
  for(size_t i=0;i<17;++i){
   bool p=false,v=false;
   for(size_t j=0;j<state_interfaces_.size();++j){
    if(state_interfaces_[j].get_name()==joints_[i]+"/position"){pi_[i]=j;p=true;}
    if(state_interfaces_[j].get_name()==joints_[i]+"/velocity"){vi_[i]=j;v=true;}
   }
   if(!p||!v)return controller_interface::CallbackReturn::ERROR;
  }
  for(size_t i=0;i<17;++i){bool found=false;
   for(size_t j=0;j<17;++j)if(command_interfaces_[j].get_name()==joints_[i]+"/effort"){ci_[i]=j;found=true;break;}
   if(!found)return controller_interface::CallbackReturn::ERROR;
  }
  if(npulse_hand_init()!=0)return controller_interface::CallbackReturn::ERROR;
  {std::lock_guard<std::mutex> lock(input_mutex_);received_=have_cached_=false;}
  applied_.fill(0);lease_=Lease{};have_coupling_rest_=false;was_allowed_=false;
  test_=HandTestSequence{};test_start_=false;test_stop_=false;automatic_session_=wall_ns();
  for(auto &command:command_interfaces_)(void)command.set_value(0.0);pub_->on_activate();active_=true;
  return controller_interface::CallbackReturn::SUCCESS;
 }
 controller_interface::CallbackReturn on_deactivate(const rclcpp_lifecycle::State &) override {
  active_=false;npulse_hand_stop();for(auto &command:command_interfaces_)(void)command.set_value(0.0);pub_->on_deactivate();
  return controller_interface::CallbackReturn::SUCCESS;
 }
 controller_interface::return_type update(const rclcpp::Time & stamp,const rclcpp::Duration &) override {
  auto now=wall_ns();
  {std::unique_lock<std::mutex> lock(input_mutex_,std::try_to_lock);
   if(lock.owns_lock()&&received_){cached_=input_;have_cached_=true;}}
  std::array<double,17> q{},v{};
  for(size_t j=0;j<17;++j){
   const auto position=state_interfaces_[pi_[j]].get_optional<double>();
   const auto velocity=state_interfaces_[vi_[j]].get_optional<double>();
   if(!position||!velocity||!std::isfinite(*position)||!std::isfinite(*velocity)){
    npulse_hand_stop();for(auto &command:command_interfaces_)(void)command.set_value(0.0);return controller_interface::return_type::ERROR;}
   q[j]=*position;v[j]=*velocity;
  }
  std::array<double,8> zeros{};
  auto coordinates=mechanics(q,v,zeros,scales_,limits_,false,0,0);
  auto ms=static_cast<uint32_t>(stamp.nanoseconds()/1000000);
  bool fresh=have_cached_&&now>=cached_.wall_ns&&now-cached_.wall_ns<=timeout_ns_;
  bool allowed=false;
  if(automatic_test_){
   allowed=test_.tick(ms,test_start_.exchange(false),test_stop_.exchange(false));
   cached_.session=automatic_session_;cached_.run=test_.started?1:0;cached_.wall_ns=now;
   cached_.enabled=allowed;cached_.revision=test_.revision();cached_.limit=automatic_limit_;
   cached_.action=HandTestSequence::stages[test_.index()].action;cached_.target_mask=255;cached_.targets.fill(0);
  }else allowed=lease_.step(fresh,cached_.enabled,cached_.session,cached_.run);
  struct npulse_hand_result result{};
  npulse_hand_step(ms,coordinates.position.data(),coordinates.velocity.data(),applied_.data(),allowed,
   cached_.session,cached_.run,cached_.revision,cached_.action,cached_.target_mask,cached_.targets.data(),cached_.limit,preserve_others_,&result);
  std::array<double,8> output{};std::copy_n(result.output,8,output.begin());
  bool new_coupling_run=!have_coupling_rest_ || coupling_session_!=cached_.session || coupling_run_!=cached_.run;
  if(allowed && result.phase==2 && (new_coupling_run || coupling_revision_!=cached_.revision)){
   const std::array<int,5> first={0,2,5,8,11},count={2,3,3,3,3};
   bool hold=new_coupling_run || cached_.action==-3;
   unsigned changed=0;
   if(cached_.action==-1)changed=cached_.target_mask;
   else if(cached_.action==-2 || (cached_.action>=0 && cached_.action<=2))changed=255;
   else if(cached_.action>=5 && cached_.action<=14)changed=1U<<((cached_.action-5)/2);
   for(size_t m=0;m<5;++m)for(int j=first[m];j<first[m]+count[m];++j){
    if(hold)coupling_rest_[j]=q[j]-coordinates.position[m];
    else if(changed&(1U<<m))coupling_rest_[j]=0;
   }
   have_coupling_rest_=true;coupling_session_=cached_.session;
   coupling_run_=cached_.run;coupling_revision_=cached_.revision;
  }
  auto coupled_q=q;
  for(size_t j=0;j<14;++j)coupled_q[j]-=coupling_rest_[j];
  auto physical=mechanics(coupled_q,v,output,scales_,limits_,allowed&&result.phase==2&&!result.causes,coupling_k_,coupling_d_);
  for(size_t j=0;j<17;++j)if(!command_interfaces_[ci_[j]].set_value(physical.torque[j])){
   npulse_hand_stop();for(auto &command:command_interfaces_)(void)command.set_value(0.0);
   return controller_interface::return_type::ERROR;
  }
  applied_=physical.applied;
  if(!automatic_test_ && was_allowed_ && !allowed){
   const char *reason=!fresh?"operator heartbeat stale":!cached_.enabled?"operator requested disable":"run expired or session changed";
   double age=have_cached_&&now>=cached_.wall_ns?(now-cached_.wall_ns)/1e6:-1.0;
   RCLCPP_WARN(get_node()->get_logger(),
    "HAND_LINK_STOP reason=%s age_ms=%.1f requested_enabled=%d session=%llu run=%u",
    reason,age,(int)cached_.enabled,(unsigned long long)cached_.session,(unsigned)cached_.run);
  }
  was_allowed_=allowed;
  if(automatic_test_){
   double shape_error=0;
   const std::array<int,5> first={0,2,5,8,11},count={2,3,3,3,3};
   for(size_t m=0;m<5;++m)for(int j=first[m];j<first[m]+count[m];++j)
    shape_error=std::max(shape_error,std::abs(q[j]-coordinates.position[m]-coupling_rest_[j]));
   test_.observe(ms,result,shape_error);
   if(!test_.event.empty()){
    if(test_.failed)RCLCPP_ERROR(get_node()->get_logger(),"%s",test_.event.c_str());
    else RCLCPP_INFO(get_node()->get_logger(),"%s",test_.event.c_str());
    test_.event.clear();
   }
  }
  State state;state.stamp=stamp;state.wall_ns=now;state.sim_ms=result.sim_ms;
  state.operator_session=cached_.session;state.operator_run=cached_.run;state.revision=cached_.revision;
  state.firmware_run=result.run;state.causes=result.causes;state.phase=result.phase;
  state.armed=result.armed;state.enabled_mask=result.enabled_mask;state.preserve_others=preserve_others_;
  std::copy_n(result.target,8,state.target.begin());std::copy_n(result.reference,8,state.reference.begin());
  std::copy_n(result.position,8,state.position.begin());std::copy_n(result.velocity,8,state.velocity.begin());
  std::copy_n(result.output,8,state.requested_percent.begin());state.applied_percent=applied_;
  std::copy_n(result.feedback_age_ms,8,state.feedback_age_ms.begin());
  std::copy_n(result.kp,8,state.kp.begin());std::copy_n(result.ki,8,state.ki.begin());std::copy_n(result.kd,8,state.kd.begin());
  state.joint_position=q;state.joint_velocity=v;state.joint_effort=physical.torque;
  {std::unique_lock<std::mutex> lock(state_mutex_,std::try_to_lock);if(lock.owns_lock()){state_=state;have_state_=true;trace_state_outcome_=test_.finished?(test_.failed?3:2):(test_.started?1:0);}}
  return controller_interface::return_type::OK;
 }
};
}
PLUGINLIB_EXPORT_CLASS(npulse_sim_control::LockstepHandController,controller_interface::ControllerInterface)
