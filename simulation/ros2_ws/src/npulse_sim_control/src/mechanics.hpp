#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace npulse_sim_control {
struct Lease {
  uint64_t active_session=0, expired_session=0;
  uint32_t active_run=0, expired_run=0;
  bool step(bool fresh,bool enabled,uint64_t session,uint32_t run) {
    bool allowed=fresh && enabled && run!=0 && !(session==expired_session&&run==expired_run);
    if(active_run && (!allowed || session!=active_session || run!=active_run)) {
      expired_session=active_session;expired_run=active_run;active_run=0;
      allowed=allowed && !(session==expired_session&&run==expired_run);
    }
    if(allowed){active_session=session;active_run=run;}
    return allowed;
  }
};
struct Mechanics {
  std::array<double,8> position{},velocity{},applied{};
  std::array<double,17> torque{};
};
inline Mechanics mechanics(const std::array<double,17>&q,const std::array<double,17>&v,
    const std::array<double,8>&u,const std::array<double,8>&scale,
    const std::array<double,17>&limits,bool allowed,double k,double d) {
  Mechanics out;
  const std::array<int,5> starts={0,2,5,8,11},counts={2,3,3,3,3};
  for(size_t m=0;m<5;++m) {
    int first=starts[m],n=counts[m];
    for(int j=first;j<first+n;++j){out.position[m]+=q[j]/n;out.velocity[m]+=v[j]/n;}
    for(int j=first;j<first+n;++j) out.torque[j]=allowed?
      u[m]*scale[m]/n-k*(q[j]-out.position[m])-d*(v[j]-out.velocity[m]):0;
  }
  out.position[5]=q[14]+q[15];out.position[6]=q[14]-q[15];out.position[7]=q[16];
  out.velocity[5]=v[14]+v[15];out.velocity[6]=v[14]-v[15];out.velocity[7]=v[16];
  if(allowed) {
    double a=u[5]*scale[5],b=u[6]*scale[6];
    out.torque[14]=a+b;out.torque[15]=a-b;out.torque[16]=u[7]*scale[7];
  }
  double factor=1;
  for(size_t j=0;j<17;++j) if(std::abs(out.torque[j])>limits[j])
    factor=std::min(factor,limits[j]/std::abs(out.torque[j]));
  for(double &t:out.torque)t*=factor;
  for(size_t m=0;m<8;++m)out.applied[m]=allowed?u[m]*factor:0;
  return out;
}
} // namespace
