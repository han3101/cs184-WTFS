#include "io.h"
#include <fstream>
#include <cmath>
#include <cstdio>

namespace cpu_lbm {
namespace io {

bool writeRaw(const std::string& path, const Field2D& fld){
  std::ofstream out(path, std::ios::binary);
  if(!out) return false;
  // write ux, uy, rho contiguous for easy compare
  out.write(reinterpret_cast<const char*>(fld.ux.data()), fld.ux.size()*sizeof(float));
  out.write(reinterpret_cast<const char*>(fld.uy.data()), fld.uy.size()*sizeof(float));
  out.write(reinterpret_cast<const char*>(fld.rho.data()), fld.rho.size()*sizeof(float));
  return out.good();
}

bool writeCSV(const std::string& path, const Field2D& fld){
  FILE* f = std::fopen(path.c_str(),"w");
  if(!f) return false;
  std::fprintf(f,"x,y,ux,uy,rho,solid\n");
  for(int y=0;y<fld.ny;++y) for(int x=0;x<fld.nx;++x){
    size_t i=fld.idx(x,y);
    std::fprintf(f,"%d,%d,%.6f,%.6f,%.6f,%d\n",x,y,fld.ux[i],fld.uy[i],fld.rho[i], fld.solid[i]?1:0);
  }
  std::fclose(f);
  return true;
}

bool writeProbeCSV(const std::string& path, const std::vector<HistoryRow>& rows){
  FILE* f = std::fopen(path.c_str(),"w");
  if(!f) return false;
  std::fprintf(f,"step,drag,lift,Cd,Cl,St\n");
  for(auto &r: rows) std::fprintf(f,"%d,%.6f,%.6f,%.6f,%.6f,%.6f\n",r.step,r.drag,r.lift,r.Cd,r.Cl,r.St);
  std::fclose(f);
  return true;
}

bool writePNG(const std::string&, const Field2D&){ return false; }

} // namespace io
} // namespace cpu_lbm
