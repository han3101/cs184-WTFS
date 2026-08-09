#include "probes.h"
#include "lattice.h"

namespace cpu_lbm {

ProbeResult computeDragLift(const Field2D& fld, float u0, float D) {
  ProbeResult r;
  float fx = 0.0f, fy = 0.0f;
  int nx=fld.nx, ny=fld.ny;
  // Momentum exchange: sum over fluid cells that have a solid neighbor
  for(int y=0;y<ny;++y) for(int x=0;x<nx;++x){
    size_t idx = fld.idx(x,y);
    if(fld.solid[idx]) continue;
    for(int q=0;q<d2q9::Q;++q){
      int sx = x + d2q9::cx[q];
      int sy = y + d2q9::cy[q];
      if(sx<0||sx>=nx||sy<0||sy>=ny) continue;
      size_t sIdx = fld.idx(sx,sy);
      if(!fld.solid[sIdx]) continue;
      // Ladd momentum exchange. q points from this fluid cell into the solid.
      // The outgoing population f_q carries momentum +c_q into the wall; the
      // bounced population f_opp arrives carrying -c_q, so removing it from the
      // fluid also transfers +c_q. The two contributions ADD:
      //   dP = c_q * (f_q + f_opp)
      int qo = d2q9::opp[q];
      float fq = fld.f[fld.fIdx(q, idx)];
      float fqo= fld.f[fld.fIdx(qo, idx)];
      fx += (fq + fqo) * float(d2q9::cx[q]);
      fy += (fq + fqo) * float(d2q9::cy[q]);
    }
  }
  r.drag = fx;
  r.lift = fy;
  float rho0 = 1.0f;
  float denom = 0.5f * rho0 * u0 * u0 * D;
  if(denom > 1e-12f){
    r.Cd = fx / denom;
    r.Cl = fy / denom;
  }
  return r;
}

void WakeProbe::sample(const Field2D& fld) {
  if(x>=0 && x<fld.nx && y>=0 && y<fld.ny){
    history.push_back(fld.uy[fld.idx(x,y)]);
  }
}

float WakeProbe::estimateFrequency() const { return estimateFrequency(dt); }

float WakeProbe::estimateSt(float u0, float D) const { return estimateSt(u0, D, dt); }

float WakeProbe::estimateFrequency(float dt) const {
  // Discard the startup transient — the flow needs time to become unsteady, and
  // counting the quiescent early samples biases the frequency low.
  size_t begin = history.size() / 2;
  size_t n = history.size() - begin;
  if(n < 10) return 0.0f;
  // Remove mean over the analysed window
  double mean=0; for(size_t i=begin;i<history.size();++i) mean+=history[i]; mean/=double(n);
  // Rising zero crossings — one per period
  int crossings=0;
  for(size_t i=begin+1;i<history.size();++i){
    float a = history[i-1]-float(mean);
    float b = history[i]-float(mean);
    if(a<0 && b>=0) crossings++;
  }
  // dt is the interval between consecutive samples, in lattice steps.
  float totalTime = float(n) * dt;
  if(totalTime<=0) return 0;
  return float(crossings) / totalTime;
}

float WakeProbe::estimateSt(float u0, float D, float dt) const {
  float f = estimateFrequency(dt);
  if(u0==0 || f==0) return 0;
  return f * D / u0;
}

} // namespace cpu_lbm
