#include <TLine.h>
#include <cstring>
#include <cstdlib>
#include <TCanvas.h>
#include <TFile.h>
#include <TH1D.h>
#include <TLegend.h>
#include <TStyle.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <iostream>

// Full-range companion to the fixed-width spectra. Paths refer to one run
// and its existing depth analysis. Units: input nm, plotted range in um.
void plot_depth_tracks_full(const char* run="output_Halite_run04",
                            const char* dir="samples/run04_depth_tracks") {
  using namespace std;
  string line; const char* names[]={"all","Na","Cl"};
  long reference[4][3]{}, tail[4][3]{}, counts[4][3]{}, above[4][3]{};
  double weight[4]{}, years=0;
  ifstream ref(string(dir)+"/depth_tracks.csv");
  if(!getline(ref,line)) throw runtime_error("Missing reference CSV");
  // Recover the original per-track weight. Check raw totals and the old
  // overflow for each species before writing the new histogram file.
  while(getline(ref,line)){
    stringstream row(line); array<string,10> v;
    for(auto& f:v) if(!getline(row,f,',')) throw runtime_error("Incomplete reference record");
    int i=(stoi(v[0])-80)/5,j=0;
    while(j<3 && v[2]!=names[j]) ++j;
    if(i<0 || i>=4 || stod(v[0])!=80+5*i || stod(v[1])!=85+5*i || j==3)
      throw runtime_error("Unexpected layer or group");
    long n=stol(v[6]); reference[i][j]+=n;
    if(v[3]=="overflow"){
      if(stod(v[4])!=25) throw runtime_error("Reference cutoff must be 25 um");
      tail[i][j]=n;
    }
    if(n && !weight[i]) weight[i]=stod(v[9])/n;
    if(!years) years=stod(v[8]);
    if(years!=stod(v[8]) || abs(stod(v[9])-n*weight[i])>1e-10*max(1.,stod(v[9])))
      throw runtime_error("Inconsistent exposure or normalization");
  }
  struct Track {int layer,z; double range;};
  vector<Track> tracks;
  ifstream source(string(run)+"/nuclei.csv");
  if(!getline(source,line) || line.find("depth_m,range_nm,range_status")==string::npos)
    throw runtime_error("Missing nuclei CSV or unexpected header");
  double smallest=INFINITY, largest=0;
  while(getline(source,line)){
    // Extract only the fields needed for the plot from the transport schema.
    const char* p=line.c_str();
    auto next=[&](){p=strchr(p,','); if(!p) throw runtime_error("Incomplete nuclei row"); ++p;};
    for(int k=0;k<3;++k) next();
    int z=atoi(p);
    for(int k=3;k<11;++k) next();
    double depth=atof(p);
    if(!(depth>=80 && depth<=100)) continue;
    next(); double range=atof(p)/1000.; next();
    if(strncmp(p,"ok,",3) || !isfinite(range) || range<=0) continue;
    int layer=min(3,int((depth-80)/5));
    tracks.push_back({layer,z,range});
    smallest=min(smallest,range); largest=max(largest,range);
  }
  if(tracks.empty()) throw runtime_error("No valid tracks");
  // Shared log bins with explicit inclusion of the measured maximum.
  constexpr int bins=100;
  double edges[bins+1];
  for(int b=0;b<=bins;b++) edges[b]=exp(log(smallest)+(log(largest)-log(smallest))*b/bins);
  edges[0]=smallest; edges[bins]=nextafter(largest,INFINITY);
  TH1D* h[4][3];
  for(int i=0;i<4;++i) for(int j=0;j<3;++j){
    h[i][j]=new TH1D(Form("h%d_%s",80+5*i,names[j]),"",bins,edges);
    h[i][j]->SetDirectory(nullptr);
  }
  for(const auto& t:tracks){
    for(int j=0;j<3;++j){
      if((j==1 && t.z!=11)||(j==2 && t.z!=17)) continue;
      h[t.layer][j]->Fill(t.range);
      ++counts[t.layer][j];
      if(t.range>25) ++above[t.layer][j];
    }
  }
  double ymin=INFINITY,ymax=0;
  for(int i=0;i<4;++i) for(int j=0;j<3;++j){
    auto hist=h[i][j];
    if(counts[i][j]!=reference[i][j] || above[i][j]!=tail[i][j] || hist->Integral()!=counts[i][j])
      throw runtime_error("Mismatch with reference or range lost to overflow");
    hist->Scale(weight[i]);
    if(weight[i]>0) ymin=min(ymin,weight[i]);
    ymax=max(ymax,hist->GetMaximum());
    cout<<80+i*5<<" m "<<names[j]<<": total "<<counts[i][j]<<", >25 um "<<above[i][j]<<endl;
  }
  cout<<"Range [um]: "<<smallest<<" to "<<largest<<endl;
  TString base=TString(dir)+"/depth_tracks_full";
  TFile out(base+".root","RECREATE");
  if(out.IsZombie()) throw runtime_error("Cannot create ROOT output");
  TCanvas c("depth_tracks_full","Full-range spectra",1440,980); c.Divide(2,2);
  const int colors[]={kBlack,kAzure+1,kOrange+7};
  for(int i=0;i<4;++i){
    c.cd(i+1); gPad->SetLogx(); gPad->SetLogy(); gPad->SetGridy();
    gPad->SetLeftMargin(.15); gPad->SetBottomMargin(.14);
    gPad->DrawFrame(smallest,pow(10,floor(log10(ymin))),edges[bins],pow(10,ceil(log10(ymax))+0.1),
      Form("%d-%d m   |   %.0f yr;CSDA range (#mum);Tracks (mg^{-1} / log bin)",80+5*i,85+5*i,years));
    TLegend legend(.66,.68,.89,.88); legend.SetBorderSize(0);
    for(int j=0;j<3;++j){
      auto hist=h[i][j]; hist->SetLineWidth(2); hist->SetLineStyle(j+1); hist->SetLineColor(colors[j]);
      hist->Write(); legend.AddEntry(hist,j==0?"All nuclei":names[j],"l");
      // Zero bins are gaps on a log axis. Draw only positive steps.
      TLine pen; pen.SetLineWidth(2); pen.SetLineColor(colors[j]); pen.SetLineStyle(j+1);
      for(int b=1;b<=bins;++b){
        double y=hist->GetBinContent(b),prev=hist->GetBinContent(b-1);
        if(y<=0) continue;
        pen.DrawLine(edges[b-1],y,edges[b],y);
        if(prev>0 && prev!=y) pen.DrawLine(edges[b-1],prev,edges[b-1],y);
      }
    }
    legend.DrawClone();
  }
  c.Write(); c.SaveAs(base+".pdf"); c.SaveAs(base+".png");
}
