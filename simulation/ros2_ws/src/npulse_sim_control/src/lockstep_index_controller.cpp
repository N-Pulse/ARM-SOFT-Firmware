// Simulation-only scheduling adapter. Production C executes inside the physics update.
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "controller_interface/controller_interface.hpp"
#include "pluginlib/class_list_macros.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_lifecycle/lifecycle_publisher.hpp"
#include "npulse_sim_control/msg/firmware_command.hpp"
#include "npulse_sim_control/msg/firmware_state.hpp"
#include "mechanics.hpp"
#include "sync_firmware.h"
namespace npulse_sim_control {
class LockstepIndexController : public controller_interface::ControllerInterface {
 using Cmd=msg::FirmwareCommand;using State=msg::FirmwareState;
 std::vector<std::string> joints_;
 std::array<size_t,17> pi_{},vi_{};
 double scale_=0.005,limit_=3;
 std::mutex input_mutex_,state_mutex_;
 Cmd input_,cached_;
 State state_;
 bool received_=false,have_cached_=false,have_state_=false;
 std::atomic<bool> active_{false};
 Lease lease_;
 uint64_t timeout_ns_=500000000;
 std::array<double,8> applied_{};
 rclcpp::Subscription<Cmd>::SharedPtr sub_;
 rclcpp_lifecycle::LifecyclePublisher<State>::SharedPtr pub_;
 rclcpp::TimerBase::SharedPtr timer_;
 static uint64_t wall_ns(){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
public:
 controller_interface::CallbackReturn on_init() override {
  auto_declare<std::vector<std::string>>("joints",{});
  auto_declare<std::vector<double>>("nm_per_percent",{0.005,0.005,0.005,0.005,0.005,0.01,0.01,0.005});
  auto_declare<std::vector<double>>("joint_limits",{});
  return controller_interface::CallbackReturn::SUCCESS;
 }
 controller_interface::InterfaceConfiguration command_interface_configuration() const override {
  return {controller_interface::interface_configuration_type::INDIVIDUAL,{"joint_index_1/effort"}};
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
  scale_=scales[1];limit_=limits[2];
  if(!std::isfinite(scale_)||scale_<=0||!std::isfinite(limit_)||limit_<=0)return controller_interface::CallbackReturn::ERROR;
  sub_=get_node()->create_subscription<Cmd>("~/firmware_command",rclcpp::QoS(1),[this](const Cmd::SharedPtr msg){
   if(!active_ || !msg->session || !std::isfinite(msg->target) || std::abs(msg->target)>2 || !msg->limit || msg->limit>100)return;
   auto now=wall_ns();if(msg->wall_ns>now || now-msg->wall_ns>timeout_ns_)return;
   std::lock_guard<std::mutex> lock(input_mutex_);
   if(received_ && (msg->session<input_.session || (msg->session==input_.session&&msg->sequence<=input_.sequence)))return;
   input_=*msg;received_=true;
  });
  pub_=get_node()->create_publisher<State>("~/firmware_state",rclcpp::QoS(1));
  timer_=get_node()->create_wall_timer(std::chrono::milliseconds(20),[this](){
   if(!active_)return;
   State copy;{std::lock_guard<std::mutex> lock(state_mutex_);if(!have_state_)return;copy=state_;}
   pub_->publish(copy); // Telemetry cannot delay the C feedback/control path.
  });
  return controller_interface::CallbackReturn::SUCCESS;
 }
 controller_interface::CallbackReturn on_activate(const rclcpp_lifecycle::State &) override {
  if(command_interfaces_.size()!=1||state_interfaces_.size()!=34)return controller_interface::CallbackReturn::ERROR;
  for(size_t i=0;i<17;++i){
   bool p=false,v=false;
   for(size_t j=0;j<state_interfaces_.size();++j){
    if(state_interfaces_[j].get_name()==joints_[i]+"/position"){pi_[i]=j;p=true;}
    if(state_interfaces_[j].get_name()==joints_[i]+"/velocity"){vi_[i]=j;v=true;}
   }
   if(!p||!v)return controller_interface::CallbackReturn::ERROR;
  }
  if(npulse_sync_init()!=0)return controller_interface::CallbackReturn::ERROR;
  {std::lock_guard<std::mutex> lock(input_mutex_);received_=have_cached_=false;}
  applied_.fill(0);lease_=Lease{};
  (void)command_interfaces_[0].set_value(0.0);pub_->on_activate();active_=true;
  return controller_interface::CallbackReturn::SUCCESS;
 }
 controller_interface::CallbackReturn on_deactivate(const rclcpp_lifecycle::State &) override {
  active_=false;npulse_sync_stop();(void)command_interfaces_[0].set_value(0.0);pub_->on_deactivate();
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
    npulse_sync_stop();(void)command_interfaces_[0].set_value(0.0);return controller_interface::return_type::ERROR;}
   q[j]=*position;v[j]=*velocity;
  }
  std::array<double,8> zeros{},scales;scales.fill(1);
  std::array<double,17> limits;limits.fill(1);
  auto coordinates=mechanics(q,v,zeros,scales,limits,false,0,0);
  coordinates.position[1]=q[2];coordinates.velocity[1]=v[2];
  bool fresh=have_cached_&&now>=cached_.wall_ns&&now-cached_.wall_ns<=timeout_ns_;
  bool allowed=lease_.step(fresh,cached_.enabled,cached_.session,cached_.run);
  struct npulse_sync_result result;
  auto ms=static_cast<uint32_t>(stamp.nanoseconds()/1000000);
  npulse_sync_step(ms,coordinates.position.data(),coordinates.velocity.data(),applied_.data(),allowed,
   cached_.session,cached_.run,cached_.target_revision,cached_.target,cached_.limit,&result);
  double torque=std::clamp(result.output*scale_,-limit_,limit_);
  if(!command_interfaces_[0].set_value(torque)){npulse_sync_stop();return controller_interface::return_type::ERROR;}
  applied_.fill(0);applied_[1]=torque/scale_;
  State state;state.stamp=stamp;state.wall_ns=now;state.sim_ms=result.sim_ms;
  state.operator_session=cached_.session;state.operator_run=cached_.run;
  state.firmware_run=result.run;state.causes=result.causes;state.phase=result.phase;
  state.armed=result.armed;state.enabled=result.enabled;state.target=result.target;
  state.reference=result.reference;state.position=result.position;state.velocity=result.velocity;
  state.requested_percent=result.output;state.applied_percent=applied_[1];state.feedback_age_ms=result.feedback_age_ms;
  state.kp=result.kp;state.ki=result.ki;state.kd=result.kd;
  {std::unique_lock<std::mutex> lock(state_mutex_,std::try_to_lock);if(lock.owns_lock()){state_=state;have_state_=true;}}
  return controller_interface::return_type::OK;
 }
};
}
PLUGINLIB_EXPORT_CLASS(npulse_sim_control::LockstepIndexController,controller_interface::ControllerInterface)
