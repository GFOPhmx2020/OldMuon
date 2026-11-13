#include <TCanvas.h>
#include <TFile.h>
#include <TH1D.h>
#include <TLegend.h>
#include <TLine.h>
#include <TLatex.h>
#include <TStyle.h>
#include <array>
#include <fstream>
#include <sstream>
#include <set>
#include <stdexcept>

// 读取深度分析 CSV。纵轴已经按样品质量和累计年份归一化。
void plot_depth_tracks(const char* dir="samples/run04_depth_tracks") {
  TString base=TString(dir)+"/depth_tracks";
  std::ifstream in((base+".csv").Data());
  std::string line;
  if (!std::getline(in,line)) throw std::runtime_error("Cannot read depth_tracks.csv");
  const char* groups[]={"all","Na","Cl"};
  const int colors[]={kBlack,kAzure+1,kOrange+7};
  TH1D* h[4][3];
  for(int i=0;i<4;++i) for(int j=0;j<3;++j){
    h[i][j]=new TH1D(Form("h%d_%s",80+5*i,groups[j]),"",100,0,25);
    h[i][j]->SetDirectory(nullptr);
    h[i][j]->SetLineColor(colors[j]);
    h[i][j]->SetLineStyle(j+1);
    h[i][j]->SetLineWidth(2);
  }
  std::set<std::array<int,3>> seen;
  double years=-1;
  while(std::getline(in,line)){
    std::stringstream row(line);
    std::array<std::string,10> v;
    for(auto& field:v) if(!std::getline(row,field,',')) throw std::runtime_error("Incomplete CSV row");
    int i=(std::stoi(v[0])-80)/5,j=0;
    while(j<3 && v[2]!=groups[j]) ++j;
    if(i<0 || i>=4 || std::stod(v[0])!=80+5*i || std::stod(v[1])!=85+5*i || j==3)
      throw std::runtime_error("Unexpected depth or element");
    auto hist=h[i][j];
    int b=101;
    if(v[3]=="visible"){
      b=hist->FindBin(std::stod(v[4]));
      if(b<1 || b>100 || hist->GetBinLowEdge(b)!=std::stod(v[4]) ||
         hist->GetBinLowEdge(b+1)!=std::stod(v[5])) throw std::runtime_error("Unexpected range bin");
    } else if(v[3]!="overflow") throw std::runtime_error("Unknown bin type");
    if(!seen.insert({i,j,b}).second) throw std::runtime_error("Duplicate bin");
    if(years<0) years=std::stod(v[8]);
    if(years!=std::stod(v[8])) throw std::runtime_error("Mixed exposures");
    hist->SetBinContent(b,std::stod(v[9]));
  }
  if(seen.size()!=1212) throw std::runtime_error("Missing bins");
  TFile out(base+".root","RECREATE");
  if(out.IsZombie()) throw std::runtime_error("Cannot create ROOT output");
  TCanvas c("depth_tracks","Depth-resolved track spectra",1400,950);
  c.Divide(2,2);
  for(int i=0;i<4;++i){
    c.cd(i+1);
    gPad->SetLogy(); gPad->SetGridy(); gPad->SetLeftMargin(.15); gPad->SetBottomMargin(.13);
    gPad->DrawFrame(0,.001,25,1000,Form("%d-%d m   |   %.0f yr;CSDA range (#mum);Tracks (mg^{-1} / bin)",80+5*i,85+5*i,years));
    TLegend legend(.65,.66,.88,.88);
    legend.SetBorderSize(0);
    for(int j=0;j<3;++j){
      auto hist=h[i][j];
      hist->Write();
      legend.AddEntry(hist,j==0?"All nuclei":groups[j],"l");
      // 只连接相邻的正值分箱，让对数坐标上的零计数保留为空白。
      TLine pen; pen.SetLineColor(colors[j]); pen.SetLineStyle(j+1); pen.SetLineWidth(2);
      for(int b=1;b<=100;++b){
        double y=hist->GetBinContent(b),x=hist->GetBinLowEdge(b);
        if(y<=0) continue;
        pen.DrawLine(x,y,hist->GetBinLowEdge(b+1),y);
        double prev=hist->GetBinContent(b-1);
        if(prev>0 && prev!=y) pen.DrawLine(x,prev,x,y);
      }
    }
    legend.DrawClone();
  }
  c.Write();
  c.SaveAs(base+".pdf"); c.SaveAs(base+".png");
}
