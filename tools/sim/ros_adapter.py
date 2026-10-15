"""Pure, ROS-independent mapping. An external model must report actual applied u.
The ROS wrapper requires an effort controller and a controller-side watchdog.
"""
from . import protocol as p

class SourceClock:
    """ROS source timestamps are translated to monotonic wall time at 1x only.
    Pauses, repeats or time jumps require STOP and a fresh bridge instance.
    """
    def __init__(self): self.stamps={}; self.anchor=None
    def capture(self,stamp_ns,ros_now_ns,wall_ms,source):
        age=(ros_now_ns-stamp_ns)//1000000
        if stamp_ns<=0 or not 0<=age<=50: raise ValueError('missing/stale model source timestamp')
        if stamp_ns<=self.stamps.get(source,0): raise ValueError('repeated/backward model source timestamp')
        if self.anchor is None: self.anchor=(stamp_ns,wall_ms)
        ros_origin,wall_origin=self.anchor
        if abs((stamp_ns-ros_origin)//1000000-(wall_ms-wall_origin))>50:
            raise ValueError('ROS source time paused/jumped or is not at wall-clock rate; restart bridge after resynchronization')
        self.stamps[source]=stamp_ns
        return wall_ms-age

class EffortAdapter:
    def __init__(self,peer,joint_names,effort_scales,publish):
        if len(joint_names)!=8 or len(set(joint_names))!=8 or len(effort_scales)!=8:
            raise ValueError('eight unique explicitly ordered actuator coordinates required')
        self.peer=peer; self.names=list(joint_names)
        self.scales=[p.finite(x) for x in effort_scales]
        if any(x<=0 for x in self.scales): raise ValueError('positive effort units per percent required')
        self.publish=publish; self.applied={}; self.forces={}
        self.last_revision=-1; self.last_permission=object()
    def applied_state(self,values,now):
        if len(values)!=8: raise ValueError('eight applied percentages required')
        prepared={i:(p.finite(v,100),now) for i,v in enumerate(values)}
        self.applied=prepared
    def applied_joint_state(self,names,efforts,captured_ms):
        if len(set(names))!=len(names): raise ValueError('duplicate applied actuator')
        prepared={}
        for mid,name in enumerate(self.names):
            if name not in names: continue
            index=names.index(name)
            if index<len(efforts): prepared[mid]=(p.finite(efforts[index]/self.scales[mid],100),captured_ms)
        self.applied={**self.applied,**prepared}
    def force_entry(self,mid,value,captured_ms):
        if mid not in range(8): raise ValueError('force ID')
        value=p.finite(value,65535)
        if value<0: raise ValueError('negative force')
        self.forces={**self.forces,mid:(round(value),captured_ms)}
    def force_state(self,values,now):
        if len(values)>8: raise ValueError('at most eight force values')
        prepared={}
        for i,value in enumerate(values):
            f=p.finite(value,65535)
            if f<0: raise ValueError('negative force')
            prepared[i]=(round(f),now)
        self.forces=prepared
    def joint_state(self,names,positions,velocities,captured_ms,now=None):
        now=captured_ms if now is None else now
        if not 0<=now-captured_ms<=p.SAMPLE_MS: raise ValueError("stale source JointState")
        oldest=captured_ms
        if len(set(names))!=len(names): raise ValueError('duplicate source joint')
        entries=[]
        for mid,name in enumerate(self.names):
            if name not in names: continue
            index=names.index(name)
            if index>=len(positions): continue
            fields=p.POSITION; pos=p.finite(positions[index]); vel=0.; force=0; applied=0.
            if index<len(velocities): fields|=p.VELOCITY; vel=p.finite(velocities[index])
            if mid in self.applied and 0<=now-self.applied[mid][1]<=p.SAMPLE_MS:
                fields|=p.APPLIED; applied=self.applied[mid][0]
                oldest=min(oldest,self.applied[mid][1])
            if mid in self.forces and 0<=now-self.forces[mid][1]<=p.SAMPLE_MS:
                fields|=p.FORCE; force=self.forces[mid][0]
                oldest=min(oldest,self.forces[mid][1])
            entries.append((mid,fields,pos,vel,force,applied))
        if entries: self.peer.submit_feedback(entries,oldest)
        return entries
    def output(self,peer,now):
        # Full array in ID order; no interpretation of arrival order or reference.
        if peer.output_revision!=self.last_revision or peer.permitted_run!=self.last_permission:
            self.last_revision=peer.output_revision; self.last_permission=peer.permitted_run
            values=peer.outputs if peer.permitted_run is not None else [0.]*8
            self.publish([values[i]*self.scales[i] for i in range(8)])
