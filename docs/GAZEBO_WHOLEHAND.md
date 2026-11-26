# Native whole-hand feedback bench

Purpose: functional firmware feedback validation, zero gravity, provisional
mechanics. This does not validate hardware timing, force sensing, grasp force,
motor electrical response, or real-world gains. ROS/Gazebo verification is needed
on the Ubuntu VM; the Mac can compile/test only the native C core.

Eight production C motor PIDs use the production hardware coefficient table
(currently Kp=10, Ki=0, Kd=0). The generated SIM initializer selects this table;
original fw/ files remain unchanged. The native HAL/RTOS boundary uses simulated
time and executes Motor_Update every 10 simulated milliseconds. Current physics
feedback reaches C directly; operator ROS traffic is not in the feedback path.

The 17 hand joint command interfaces are effort. Only joint_thumb_1_y remains
position-supported, because the eight-motor model has no independent actuator
for this extra thumb degree of freedom. Base joints stay fixed as in the bench.
Finger motor coordinates are mean phalange angles. Internal relative springs
and damping maintain simplified equal-angle phalange coupling; they do not
anchor any finger to a commanded target. The firmware PID controls the mean.
Individual joint positions are recorded to detect averaging that hides a fault.
Wrist motor positions are qx+qy and qx-qy; torques use the transpose mapping.
The inherited motor scale, masses, coupling and thumb/palm map are not calibrated.

Named commands invoke the production IntentRouter in the C adapter. Neutral
comes from MotorMap_GetPose. Original SIM UART accepts only intent IDs 0..4;
this native adapter dispatches 0..16 directly after armed/all-enabled checks,
so it does not validate wire compatibility for the additional commands.

IMPORTANT semantic adaptation, preserve_others=true: original single-digit
commands are full poses resetting other axes to neutral. The adapter saves the
old targets, invokes the C router, then restores unaffected motor targets and
applies the operator ceiling before Motor_Update. Wrist-only commands likewise
preserve fingers. This is deliberately not a claim that the unmodified router
already implements S1 semantics. Set preserve_others=false in the new YAML and
restart Gazebo if exact original full-pose semantics are desired.

REST means no target change, PID still holding. STOP releases efforts. Loss of
operator lease (500 wall ms) or fault zeroes efforts and requires explicit START.
GUARD remains enabled (five simulated seconds without movement under drive).
No firmware protection is disabled. Pausing/resetting/reloading physics requires
caution: use STOP, and restart Gazebo for a fresh bench/session after reset.

PINCH is the existing C pose, not a validated precision grasp on an object.
POWER_GRASP/CYLINDRICAL_GRASP is explicitly an alias of existing CLOSE_HAND,
not a newly implemented force-controlled cylindrical grasp.
WRIST_PALM/DORSI use the existing C mapping; verify their visible anatomical
directions in Gazebo rather than assuming the model's axis labels establish it.

Run gazebo_hand_lockstep.launch.py, then tools/sim_hand.py --limit 15 --csv PATH.
Never run the previous native console/bridge simultaneously. test commands a
sequence of poses and checks <2 degrees error, <2 degrees phalange spread and
<0.05 rad/s speed for 0.5 simulated second. A timeout/fault requests STOP; it
never forces positions or automatically recovers. This is sampled convergence
evidence, not validation of all transient behavior or physical grasping.

CSV records actual C targets/reference/measurement/efforts/ages/gains and all
17 joints. plot_hand_feedback.py exports PNG/PDF position/error/effort curves.
An optional second CSV can be overlaid but is not automatically an open-loop
trial. Do not label the old rotor-feedback system as "without feedback": it was
closed on a different motor model. A valid open-loop/perturbation experiment
still needs a defined baseline and matched trials; it is not implemented here.
