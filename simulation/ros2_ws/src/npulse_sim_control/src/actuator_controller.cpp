// Simulation prototype: no hardware or hard-real-time certification.
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "controller_interface/controller_interface.hpp"
#include "mechanics.hpp"
#include "pluginlib/class_list_macros.hpp"
#include "npulse_sim_control/msg/actuator_command.hpp"
#include "npulse_sim_control/msg/actuator_state.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_lifecycle/lifecycle_publisher.hpp"

namespace npulse_sim_control {
using CallbackReturn = controller_interface::CallbackReturn;
using Command = msg::ActuatorCommand;
using State = msg::ActuatorState;
static uint64_t steady_ns() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count();
}
class ActuatorController : public controller_interface::ControllerInterface {
  std::vector<std::string> joints_;
  std::vector<double> limits_, scales_;
  std::array<size_t,17> ci_{}, pi_{}, vi_{};
  std::mutex command_mutex_, state_mutex_;
  Command pending_, cached_;
  State state_;
  bool have_command_{false}, have_cached_{false}, have_state_{false};
  std::atomic<bool> active_{false};
  Lease lease_;
  bool index_only_{false};
  uint64_t timeout_ns_{500000000};
  double coupling_k_{0.3}, coupling_d_{0.01};
  rclcpp::Subscription<Command>::SharedPtr sub_;
  rclcpp_lifecycle::LifecyclePublisher<State>::SharedPtr pub_;
  rclcpp::TimerBase::SharedPtr timer_;
  // Order is fixed by the generated profile and checked at configuration.
  const std::vector<std::string> expected_ = {
    "joint_thumb_2","joint_thumb_3","joint_index_1","joint_index_2","joint_index_3",
    "joint_middle_1","joint_middle_2","joint_middle_3","joint_ring_1","joint_ring_2",
    "joint_ring_3","joint_little_1","joint_little_2","joint_little_3",
    "joint_wrist_x","joint_wrist_y","joint_thumb_1_x"};
  void zero() { for (auto & interface : command_interfaces_) { (void)interface.set_value(0.0); } }
public:
  CallbackReturn on_init() override {
    auto_declare<std::vector<std::string>>("joints", {});
    auto_declare<std::vector<double>>("joint_limits", {});
    auto_declare<std::vector<double>>("nm_per_percent", {0.005,0.005,0.005,0.005,0.005,0.01,0.01,0.005});
    auto_declare<bool>("index_only", false);
    auto_declare<double>("command_timeout_s", 0.5);
    auto_declare<double>("coupling_stiffness", 0.3);
    auto_declare<double>("coupling_damping", 0.01);
    return CallbackReturn::SUCCESS;
  }
  controller_interface::InterfaceConfiguration command_interface_configuration() const override {
    controller_interface::InterfaceConfiguration out;
    out.type=controller_interface::interface_configuration_type::INDIVIDUAL;
    for(size_t i=0;i<joints_.size();++i) if(!index_only_ || i==2) out.names.push_back(joints_[i]+"/effort");
    return out;
  }
  controller_interface::InterfaceConfiguration state_interface_configuration() const override {
    controller_interface::InterfaceConfiguration out;
    out.type=controller_interface::interface_configuration_type::INDIVIDUAL;
    for (const auto & j : joints_) { out.names.push_back(j+"/position"); out.names.push_back(j+"/velocity"); }
    return out;
  }
  CallbackReturn on_configure(const rclcpp_lifecycle::State &) override {
    index_only_=get_node()->get_parameter("index_only").as_bool();
    joints_=get_node()->get_parameter("joints").as_string_array();
    limits_=get_node()->get_parameter("joint_limits").as_double_array();
    scales_=get_node()->get_parameter("nm_per_percent").as_double_array();
    double timeout=get_node()->get_parameter("command_timeout_s").as_double();
    coupling_k_=get_node()->get_parameter("coupling_stiffness").as_double();
    coupling_d_=get_node()->get_parameter("coupling_damping").as_double();
    auto positive=[](double x){return std::isfinite(x)&&x>0;};
    if(joints_!=expected_ || limits_.size()!=17 || scales_.size()!=8 ||
       !std::all_of(limits_.begin(),limits_.end(),positive) || !std::all_of(scales_.begin(),scales_.end(),positive) ||
       !positive(timeout) || timeout>1 || !std::isfinite(coupling_k_) || coupling_k_<0 ||
       !std::isfinite(coupling_d_) || coupling_d_<0) return CallbackReturn::ERROR;
    timeout_ns_=static_cast<uint64_t>(timeout*1e9);
    sub_=get_node()->create_subscription<Command>("~/commands",rclcpp::QoS(1),[this](const Command::SharedPtr m){
      if(!active_ || m->session==0) return;
      const auto now=steady_ns();
      if(m->monotonic_ns>now || now-m->monotonic_ns>timeout_ns_) return;
      for(double x:m->percent) if(!std::isfinite(x)||std::abs(x)>100) return;
      std::lock_guard<std::mutex> lock(command_mutex_);
      if(have_command_ && (m->session<pending_.session ||
          (m->session==pending_.session && m->sequence<=pending_.sequence))) return;
      pending_=*m; have_command_=true;
    });
    pub_=get_node()->create_publisher<State>("~/state",rclcpp::QoS(1));
    timer_=get_node()->create_wall_timer(std::chrono::milliseconds(10),[this](){
      if(!active_) return;
      State copy;
      { std::lock_guard<std::mutex> lock(state_mutex_); if(!have_state_) return; copy=state_; }
      // Do not re-stamp: a stopped physics/control loop yields expired state.
      pub_->publish(copy);
    });
    return CallbackReturn::SUCCESS;
  }
  CallbackReturn on_activate(const rclcpp_lifecycle::State &) override {
    if(command_interfaces_.size()!=(index_only_?1u:17u) || state_interfaces_.size()!=34) return CallbackReturn::ERROR;
    for(size_t i=0;i<17;++i) {
      bool c=index_only_ && i!=2,p=false,v=false;
      for(size_t k=0;k<command_interfaces_.size();++k) if(command_interfaces_[k].get_name()==joints_[i]+"/effort") {ci_[i]=k;c=true;}
      for(size_t k=0;k<state_interfaces_.size();++k) {
        if(state_interfaces_[k].get_name()==joints_[i]+"/position") {pi_[i]=k;p=true;}
        if(state_interfaces_[k].get_name()==joints_[i]+"/velocity") {vi_[i]=k;v=true;}
      }
      if(!c||!p||!v) return CallbackReturn::ERROR;
    }
    {std::lock_guard<std::mutex> lock(command_mutex_);have_command_=have_cached_=false;}
    {std::lock_guard<std::mutex> lock(state_mutex_);have_state_=false;}
    lease_=Lease{};
    zero(); pub_->on_activate(); active_=true;
    return CallbackReturn::SUCCESS;
  }
  CallbackReturn on_deactivate(const rclcpp_lifecycle::State &) override {
    active_=false; zero(); pub_->on_deactivate(); return CallbackReturn::SUCCESS;
  }
  controller_interface::return_type update(const rclcpp::Time & stamp,const rclcpp::Duration &) override {
    const auto now=steady_ns();
    {std::unique_lock<std::mutex> lock(command_mutex_,std::try_to_lock);
      if(lock.owns_lock()&&have_command_) {cached_=pending_;have_cached_=true;}}
    std::array<double,17> q{},v{};
    for(size_t i=0;i<17;++i) {
      q[i]=state_interfaces_[pi_[i]].get_value(); v[i]=state_interfaces_[vi_[i]].get_value();
      if(!std::isfinite(q[i])||!std::isfinite(v[i])) {zero();return controller_interface::return_type::ERROR;}
    }
    bool fresh=have_cached_ && now>=cached_.monotonic_ns && now-cached_.monotonic_ns<=timeout_ns_;
    const bool allowed=lease_.step(fresh,cached_.enabled,cached_.session,cached_.run);
    std::array<double,8> scales;
    std::array<double,17> limits;
    std::copy(scales_.begin(),scales_.end(),scales.begin());
    std::copy(limits_.begin(),limits_.end(),limits.begin());
    auto model=mechanics(q,v,cached_.percent,scales,limits,allowed,coupling_k_,coupling_d_);
    if(index_only_) {
      // Isolated hinge: motor 1 maps directly to index MCP. All other joints
      // are independently held by the position support controller.
      model=mechanics(q,v,cached_.percent,scales,limits,false,0,0);
      model.position[1]=q[2];model.velocity[1]=v[2];
      double torque=allowed?std::clamp(cached_.percent[1]*scales[1],-limits[2],limits[2]):0;
      model.torque[2]=torque;model.applied[1]=torque/scales[1];
    }
    State s; s.stamp=stamp; s.monotonic_ns=now;
    s.session=have_cached_?cached_.session:0;s.run=have_cached_?cached_.run:0;s.permitted=allowed;
    s.motor_mask=index_only_?2:255;
    s.position=model.position;s.velocity=model.velocity;s.applied_percent=model.applied;
    s.joint_effort=model.torque;
    for(size_t j=0;j<17;++j) {
      if(index_only_ && j!=2) continue;
      if(!command_interfaces_[ci_[j]].set_value(model.torque[j])) {zero();return controller_interface::return_type::ERROR;}
    }
    {std::unique_lock<std::mutex> lock(state_mutex_,std::try_to_lock);
      if(lock.owns_lock()){state_=s;have_state_=true;}}
    return controller_interface::return_type::OK;
  }
};
} // namespace
PLUGINLIB_EXPORT_CLASS(npulse_sim_control::ActuatorController, controller_interface::ControllerInterface)
