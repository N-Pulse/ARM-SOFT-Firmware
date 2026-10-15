#!/usr/bin/env python3
"""SIM v2 effort adapter. ROS imports and serial opening occur only in main().
No Gazebo model, wrist transform or position-controller fallback is invented.
"""
import argparse
import math
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from sim.client import SerialWorker, submit_command, milliseconds
from sim.ros_adapter import EffortAdapter, SourceClock

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--transport',choices=('serial','native'),default='serial')
    parser.add_argument('--port')
    parser.add_argument('--native-binary',help='explicit executable built by tools/build_native_sim.py')
    parser.add_argument('--use-sim-time',action='store_true',help='use ROS /clock; source time must still advance at wall-clock rate')
    parser.add_argument('--joint-names',nargs=8,required=True)
    parser.add_argument('--effort-per-percent',nargs=8,type=float,required=True)
    parser.add_argument('--effort-topic',required=True)
    parser.add_argument('--applied-effort-topic',required=True,help='stamped JointState: names and ACTUAL applied effort, same units/scales as controller')
    parser.add_argument('--model-watchdog-ms',type=int,required=True,
                        help='existing independent model/controller watchdog, <=100 ms; declaration is not verification')
    parser.add_argument('--force-topics',nargs=8,help='optional eight stamped Wrench topics, IDs 0..7; vector norm N converted to mN')
    args=parser.parse_args()
    if args.transport=='serial' and not args.port: parser.error('--port required for serial')
    if args.transport=='native' and (not args.native_binary or args.port): parser.error('native requires --native-binary and no --port')
    if len(set(args.joint_names))!=8 or any(not math.isfinite(x) or x<=0 for x in args.effort_per_percent):
        parser.error('eight unique coordinates and positive finite effort scales required')
    if not 1<=args.model_watchdog_ms<=100:
        parser.error('an independent effort-controller/model watchdog <=100 ms is required')
    import rclpy
    from rclpy.node import Node
    from rclpy.parameter import Parameter
    from sensor_msgs.msg import JointState
    from std_msgs.msg import Float64MultiArray, String
    from geometry_msgs.msg import WrenchStamped
    class SerialBridge(Node):
        def __init__(self):
            super().__init__('n_pulse_sim_v2',parameter_overrides=[Parameter('use_sim_time',Parameter.Type.BOOL,args.use_sim_time)])
            self.source_clock=SourceClock()
            self.publisher=self.create_publisher(Float64MultiArray,args.effort_topic,1)
            transport=None
            if args.transport=='native':
                from sim.native_transport import NativeTransport
                transport=NativeTransport(args.native_binary)
            self.worker=SerialWorker(args.port,transport=transport)
            self.adapter=EffortAdapter(self.worker.peer,args.joint_names,args.effort_per_percent,self.publish_effort)
            self.worker.on_state=self.adapter.output
            self.create_subscription(JointState,'/joint_states',self.joints,1)
            self.create_subscription(JointState,args.applied_effort_topic,self.applied,1)
            self.create_subscription(String,'/n_pulse/sim_control',self.control,8)
            if args.force_topics:
                for mid,topic in enumerate(args.force_topics):
                    self.create_subscription(WrenchStamped,topic,lambda msg,mid=mid:self.force(mid,msg),1)
            self.worker.start()
        def publish_effort(self,values):
            msg=Float64MultiArray(); msg.data=values; self.publisher.publish(msg)
        def captured(self,msg,source):
            stamp=msg.header.stamp.sec*1000000000+msg.header.stamp.nanosec
            return self.source_clock.capture(stamp,self.get_clock().now().nanoseconds,milliseconds(),source)
        def joints(self,msg):
            try: self.adapter.joint_state(list(msg.name),list(msg.position),list(msg.velocity),self.captured(msg,'joints'),milliseconds())
            except ValueError as exc: self.worker.peer.request(8); self.get_logger().error(str(exc))
        def applied(self,msg):
            try: self.adapter.applied_joint_state(list(msg.name),list(msg.effort),self.captured(msg,'applied'))
            except ValueError as exc: self.worker.peer.request(8); self.get_logger().error(str(exc))
        def force(self,mid,msg):
            try:
                f=msg.wrench.force
                self.adapter.force_entry(mid,math.sqrt(f.x*f.x+f.y*f.y+f.z*f.z)*1000,self.captured(msg,('force',mid)))
            except ValueError as exc: self.worker.peer.request(8); self.get_logger().error(str(exc))
        def control(self,msg):
            try:
                queued=submit_command(self.worker.peer,msg.data)
                self.get_logger().info('queued' if queued else 'refused/full; not delivered')
            except ValueError as exc: self.get_logger().error(str(exc))
    rclpy.init(); bridge=SerialBridge()
    try: rclpy.spin(bridge)
    finally:
        bridge.worker.close(); bridge.publish_effort([0.]*8)
        bridge.destroy_node(); rclpy.shutdown()
if __name__=='__main__': main()
