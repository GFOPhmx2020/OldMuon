// muon_beam.cc — 单能 μ 子束流入射立方体矿物：完整输运 + ROOT 分析。
// 单线程设计：确保事件编号、CSV 和 ROOT 对象写入顺序明确，不引入线程共享输出。
// 构建/运行见同目录 README_muon_beam.md。
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
constexpr double PI=3.14159265358979323846;

#include "G4RunManager.hh"
#include "G4VUserDetectorConstruction.hh"
#include "G4VUserPrimaryGeneratorAction.hh"
#include "G4UserTrackingAction.hh"
#include "G4ParticleGun.hh"
#include "G4ParticleTable.hh"
#include "G4NistManager.hh"
#include "G4Material.hh"
#include "G4Box.hh"
#include "G4Tubs.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4SystemOfUnits.hh"
#include "G4PhysicalConstants.hh"
#include "G4VPhysicalVolume.hh"
#include "G4Event.hh"
#include "G4Track.hh"
#include "G4VProcess.hh"
#include "G4Version.hh"
#include "QGSP_BIC_HP.hh"
#include "G4EmStandardPhysics_option4.hh"
#include "G4IonTable.hh"
#include "Randomize.hh"
#include "TFile.h"
#include "TH2D.h"
#include "TCanvas.h"
#include "TStyle.h"
#include "TColor.h"
#include "TROOT.h"

// 模块 3：集中配置。位置用米，记录阈值用 MeV；100000 是初级事件数，
// 每次事件仅发射一个 μ 子，并非核数。材料配置文件由项目 YAML 转换。
// 所有长度内部以 m 存储；卡片边长和束斑用 cm，避免实验尺寸单位混淆。
struct Config {
    std::string material="Halite",particle="mu-",out="output_beam",data="..",materials="materials.tsv",cardText;
    int events=100000,progress=1000,xBins=80,zBins=100,rangeBins=100;
    long seed=20261101;
    // 核电荷数、质量数、初始离子电荷态；默认值表示尚未配置。
    int ionZ = 0;
    int ionA = 0;
    int ionQ = 0;
    double length=.10,radius=.05,beamRadius=.03,energy=100,thresholdMeV=.001,cutMM=.005,rangeMin=.001,rangeMax=1e7;
};
Config readCard(const std::string& filename){
    Config c;auto path=std::filesystem::absolute(filename);std::ifstream in(path);
    if(!in)throw std::runtime_error("Cannot open configuration card");
    std::map<std::string,bool> seen;std::string line;
    while(std::getline(in,line)){
        c.cardText+=line+"\n";auto hash=line.find('#');if(hash!=std::string::npos)line.resize(hash);
        std::istringstream row(line);std::string key,value,extra;if(!(row>>key))continue;
        if(!(row>>std::quoted(value))||(row>>extra)||seen[key])throw std::runtime_error("Invalid/duplicate card entry: "+key);seen[key]=true;
        auto real=[&](){std::size_t used;double v=std::stod(value,&used);if(used!=value.size()||!std::isfinite(v))throw std::runtime_error("Invalid numeric value");return v;};
        auto integer=[&](){double v=real();if(v<1||v>100000000||v!=std::floor(v))throw std::runtime_error("Expected positive integer");return int(v);};
        auto resolve=[&](){return (path.parent_path()/value).lexically_normal().string();};
        if(key=="/beam/material")c.material=value;
        else if(key=="/beam/particle")c.particle=value;
        else if(key=="/beam/energy_GeV")c.energy=real();
        else if(key=="/beam/side_cm")c.length=real()/100;
        else if(key=="/beam/spotRadius_cm")c.beamRadius=real()/100;
        else if(key=="/beam/events")c.events=integer();
        else if(key=="/beam/seed")c.seed=integer();
        else if(key=="/beam/output")c.out=resolve();
        else if(key=="/beam/data")c.data=resolve();
        else if(key=="/beam/materials")c.materials=resolve();
        else if(key=="/beam/recordThreshold_MeV")c.thresholdMeV=real();
        else if(key=="/beam/productionCut_mm")c.cutMM=real();
        else if(key=="/beam/xBins")c.xBins=integer();
        else if(key=="/beam/zBins")c.zBins=integer();
        else if(key=="/beam/ionZ") c.ionZ=integer();
        else if(key=="/beam/ionA") c.ionA=integer();
        else if(key=="/beam/ionQ") c.ionQ=integer();
        else throw std::runtime_error("Unknown command: "+key);
    }
    if(c.particle!="mu-" && c.particle!="mu+" &&
        c.particle!="proton" && c.particle!="ion")
         throw std::runtime_error(
            "particle must be mu-, mu+, proton or ion");

    if(c.particle=="ion" &&
        (c.ionZ<1 || c.ionA<c.ionZ ||
        c.ionQ<1 || c.ionQ>c.ionZ))
        throw std::runtime_error("Invalid ion Z, A or charge state");
    if(c.energy<=0||c.length<=0||c.beamRadius<=0||c.beamRadius>c.length/2||c.thresholdMeV<0||c.cutMM<=0||1LL*c.xBins*c.zBins>1000000)
        throw std::runtime_error("Invalid geometry, energy, threshold or bin count");
    c.radius=c.length/2; // 复用 XZ 绘图范围字段，含义是立方体半边长，不是圆柱半径。
    for(auto k:{"output","data","materials"})if(!seen[std::string("/beam/")+k])throw std::runtime_error("Missing required path");
    return c;
}

// 材料表格式：名称 密度(g/cm3) 元素1 原子相对数1 ...。
// 按原子量转换质量分数，支持 Olivine 非整数化学计量，不用整数重载截断。
G4Material* loadMaterial(const Config& cfg) {
    // StdRock 在旧工程按质量分数定义，单独处理以保持原定义精确。
    if(cfg.material=="StdRock") {
        auto mat=new G4Material("StdRock",2.65*g/cm3,4);
        auto nist=G4NistManager::Instance();
        mat->AddElement(nist->FindOrBuildElement("O"),.52);
        mat->AddElement(nist->FindOrBuildElement("Ca"),.27);
        mat->AddElement(nist->FindOrBuildElement("C"),.12);
        mat->AddElement(nist->FindOrBuildElement("Mg"),.09);
        return mat;
    }
    std::ifstream in(cfg.materials); if(!in) throw std::runtime_error("Cannot open materials.tsv");
    std::string line;
    while(std::getline(in,line)){
        if(line.empty()||line[0]=='#')continue;
        std::istringstream s(line);std::string name,el;double density,n;
        s>>name>>density;if(name!=cfg.material)continue;
        std::vector<std::pair<G4Element*,double>> components;double sum=0;
        while(s>>el>>n){auto e=G4NistManager::Instance()->FindOrBuildElement(el);if(!e||n<=0)throw std::runtime_error("Bad material");components.push_back({e,n});sum+=e->GetA()*n;}
        if(components.empty()||density<=0)throw std::runtime_error("Empty material");
        auto mat=new G4Material(name,density*g/cm3,components.size());
        for(auto [e,q]:components)mat->AddElement(e,e->GetA()*q/sum);
        return mat;
    }
    throw std::runtime_error("Unknown material: "+cfg.material);
}
class Detector:public G4VUserDetectorConstruction {
    Config cfg;
public:
    explicit Detector(Config c):cfg(c){}
    G4VPhysicalVolume* Construct() override {
        // 世界为真空：本程序模拟实验单能束流，不进行上游大气输运。
        auto vacuum=G4NistManager::Instance()->FindOrBuildMaterial("G4_Galactic");
        double half=std::max(cfg.length,cfg.radius)*1.2*m;
        auto world=new G4LogicalVolume(new G4Box("World",half,half,half),vacuum,"World");
        auto physical=new G4PVPlacement(nullptr,{},world,"World",nullptr,false,0,true);
        auto target=new G4LogicalVolume(new G4Box("Target",cfg.length*m/2,cfg.length*m/2,cfg.length*m/2),loadMaterial(cfg),"Target");
        new G4PVPlacement(nullptr,{},target,"Target",world,false,0,true);
        return physical;
    }
};

// 模块 4：按同位素读取项目射程表。第四列 Range_um 是已有 CSDA 射程，
// 与实际曲折路径长度、可蚀刻损伤径迹并不等同。采用线性插值以对应现有 Python。
// 缺文件/超范围返回状态，而非任意外推；有效端点也可直接使用。
struct RangeTable {std::vector<double> energy,nm;};
class Ranges {
    std::string directory;std::map<std::pair<int,int>,RangeTable> cache;
public:
    explicit Ranges(const Config& c):directory(c.data+"/"+c.material+"/Range/"){}
    std::pair<double,std::string> get(int Z,int A,double E){
        auto key=std::make_pair(Z,A);
        if(!cache.count(key)){
            RangeTable t;std::ifstream in(directory+"DEDX_Z"+std::to_string(Z)+"_A"+std::to_string(A)+".txt");
            std::string line;std::getline(in,line);double e,se,sn,r;
            while(in>>e>>se>>sn>>r){
                if(!std::isfinite(e)||!std::isfinite(r)||r<0||(!t.energy.empty()&&e<=t.energy.back()))throw std::runtime_error("Invalid range table");
                t.energy.push_back(e);t.nm.push_back(r*1000);
            }
            cache[key]=std::move(t);
        }
        const auto& t=cache.at(key);
        if(t.energy.size()<2)return {-1,"missing_table"};
        if(E<t.energy.front()||E>t.energy.back())return {-1,"outside_table"};
        auto k=std::lower_bound(t.energy.begin(),t.energy.end(),E)-t.energy.begin();
        if(k==0)return {t.nm[0],"ok"};
        double f=(E-t.energy[k-1])/(t.energy[k]-t.energy[k-1]);
        return {t.nm[k-1]+f*(t.nm[k]-t.nm[k-1]),"ok"};
    }
};

// 模块 5：逐核流式 CSV 与按元素 ROOT 直方图。X-Z 是沿 Y 投影的面密度；
// z–range 的第三轴是 bin 计数 N，不除射程 bin 宽度。全部范围外计数仍存 CSV。
struct Plots {std::unique_ptr<TH2D> xz,zrange;long nuclei=0,validRange=0,missing=0,outside=0,plotOutside=0;};
class Output {
public:
    Config cfg;Ranges ranges;std::ofstream nuclei,primaries;std::map<int,Plots> plots;
    explicit Output(Config c):cfg(c),ranges(c){
        if(std::filesystem::exists(cfg.out))throw std::runtime_error("Output directory exists; choose a new path");
        std::filesystem::create_directories(cfg.out);
        nuclei.open(cfg.out+"/nuclei.csv");primaries.open(cfg.out+"/primaries.csv");
        if(!nuclei||!primaries)throw std::runtime_error("Cannot open CSV output");
        nuclei<<std::setprecision(12)<<"event,track,parent,Z,A,particle,process,E_MeV,x_m,y_m,z_m,depth_m,range_nm,range_status,dir_x,dir_y,dir_z\n";
        primaries<<std::setprecision(12)<<"event,charge,E_GeV,cos_theta,phi_rad,x_m,y_m,z_m\n";
    }
    void record(const G4Track* t){
        auto def=t->GetDefinition();int Z=def->GetAtomicNumber(),A=def->GetAtomicMass();
        double E=t->GetKineticEnergy()/MeV;
        // 有核电荷和质量数的次级（含质子），排除电子、中子；不杀死任何轨迹。
        // PreUserTrackingAction 每条轨迹调用一次（本程序没有 suspend/retracking）。
        if(t->GetParentID()==0||Z<1||A<1||E<=cfg.thresholdMeV)return;
        auto vol=t->GetVolume();if(!vol||vol->GetName()!="Target")return;
        // 在第一次输运之前取得出生动量单位方向，不能用后续散射方向替代。
        const auto direction=t->GetMomentumDirection();
        auto pos=t->GetPosition();auto [range,status]=ranges.get(Z,A,E);
        auto proc=t->GetCreatorProcess();std::string process=proc?proc->GetProcessName():"unknown";
        int event=G4RunManager::GetRunManager()->GetCurrentEvent()->GetEventID();
        nuclei<<event<<','<<t->GetTrackID()<<','<<t->GetParentID()<<','<<Z<<','<<A<<','<<def->GetParticleName()<<','<<process<<','<<E<<','
              <<pos.x()/m<<','<<pos.y()/m<<','<<pos.z()/m<<','<<cfg.length/2-pos.z()/m<<','<<range<<','<<status<<','<<direction.x()<<','<<direction.y()<<','<<direction.z()<<'\n';
        auto& p=plots[Z];if(!p.xz){
            std::string tag="Z"+std::to_string(Z);
            p.xz=std::make_unique<TH2D>((tag+"_xz_counts").c_str(),(tag+";X [m];Z [m];N").c_str(),cfg.xBins,-cfg.radius,cfg.radius,cfg.zBins,-cfg.length/2,cfg.length/2);
            std::vector<double> le(cfg.rangeBins+1);for(int i=0;i<=cfg.rangeBins;++i)le[i]=cfg.rangeMin*std::pow(cfg.rangeMax/cfg.rangeMin,double(i)/cfg.rangeBins);
            std::vector<double> ze(cfg.zBins+1);for(int i=0;i<=cfg.zBins;++i)ze[i]=-cfg.length/2+i*cfg.length/cfg.zBins;
            p.zrange=std::make_unique<TH2D>((tag+"_z_range_counts").c_str(),(tag+";z [m];CSDA range [nm];N / bin").c_str(),cfg.zBins,ze.data(),cfg.rangeBins,le.data());
            p.xz->SetDirectory(nullptr);p.zrange->SetDirectory(nullptr);
        }
        ++p.nuclei;p.xz->Fill(pos.x()/m,pos.z()/m);
        if(status=="ok") {++p.validRange;p.zrange->Fill(pos.z()/m,range);if(range<cfg.rangeMin||range>=cfg.rangeMax)++p.plotOutside;}
        else if(status=="missing_table")++p.missing;else ++p.outside;
    }
    void finish(){
        nuclei.flush();primaries.flush();if(!nuclei||!primaries)throw std::runtime_error("CSV write failed");
        TFile file((cfg.out+"/muon_beam.root").c_str(),"RECREATE");if(file.IsZombie())throw std::runtime_error("ROOT open failed");
        double stops[]={0,.25,.5,.75,1},r[]={.96,.76,.42,.18,.04},g[]={.98,.86,.68,.46,.22},b[]={1,.94,.84,.70,.44};
        TColor::CreateGradientColorTable(5,stops,r,g,b,255);gStyle->SetNumberContours(255);gStyle->SetOptStat(0);
        std::ofstream summary(cfg.out+"/summary.csv");summary<<"Z,nuclei,valid_range,missing_table,outside_table,outside_range_plot\n";
        for(auto& [Z,p]:plots){
            summary<<Z<<','<<p.nuclei<<','<<p.validRange<<','<<p.missing<<','<<p.outside<<','<<p.plotOutside<<'\n';
            p.xz->Write();p.zrange->Write();
            auto density=std::unique_ptr<TH2D>(static_cast<TH2D*>(p.xz->Clone(("Z"+std::to_string(Z)+"_xz_density").c_str())));
            density->SetDirectory(nullptr);density->Scale(1.,"width");density->GetZaxis()->SetTitle("N / m^{2} (Y projected)");density->Write();
            TCanvas canvas("plot","",1100,800);canvas.SetRightMargin(.19);canvas.SetBottomMargin(.14);
            density->Draw("COLZ");
            std::string prefix=cfg.out+"/Z"+std::to_string(Z);
            canvas.SaveAs((prefix+"_xz.png").c_str());canvas.SaveAs((prefix+"_xz.pdf").c_str());
            canvas.Clear();canvas.SetLogy();canvas.SetTheta(28);canvas.SetPhi(40);
            p.zrange->Draw("SURF2");canvas.SaveAs((prefix+"_z_range.png").c_str());canvas.SaveAs((prefix+"_z_range.pdf").c_str());
        }
        file.Close();
        // 单能实验束流无给定粒子流率，不能用大气 GT 换算自然时间。
        std::ofstream meta(cfg.out+"/metadata.txt");
        meta<<std::setprecision(15)<<"Geant4="<<G4Version<<"\nmaterial="<<cfg.material<<"\nparticle="<<cfg.particle
            <<"\nevents="<<cfg.events<<"\nseed="<<cfg.seed<<"\nside_m="<<cfg.length<<"\nlength_m="<<cfg.length
            <<"\nbeam_radius_m="<<cfg.beamRadius<<"\nenergy_GeV="<<cfg.energy<<"\nrecord_threshold_MeV="<<cfg.thresholdMeV
            <<"\nphysics=QGSP_BIC_HP + EM option4\ndirection=0,0,-1\ngeometry=cube\nenergy_definition=kinetic\n";
        if(!summary||!meta)throw std::runtime_error("Metadata write failed");
    }
};

// 模块 6：单能束流，sqrt(U) 保证束斑在圆盘面积上均匀。
// 固定 -z 方向，源位于顶面上方 1 um 真空，x/y 不因传播发生偏移。
class Gun:public G4VUserPrimaryGeneratorAction {
    Output& out;G4ParticleGun gun{1};
public:
    explicit Gun(Output& o):out(o){}
    void GeneratePrimaries(G4Event* event) override {
        double az=2*PI*G4UniformRand(),r=out.cfg.beamRadius*std::sqrt(G4UniformRand());
        double x=r*std::cos(az),y=r*std::sin(az),z=out.cfg.length/2+1e-6;
        G4ParticleDefinition* definition = nullptr;

        if(out.cfg.particle=="ion") {
            // 第三个参数为核激发能；0 表示基态。
            definition = G4IonTable::GetIonTable()->GetIon(
                out.cfg.ionZ, out.cfg.ionA, 0.0);
        } else {
            definition =
                G4ParticleTable::GetParticleTable()->FindParticle(
                    out.cfg.particle);
        }

        if(!definition)
            throw std::runtime_error("Particle definition not found");

        gun.SetParticleDefinition(definition);

        // 核的 Z 与初始离子电荷态 Q 是不同概念。
        const double charge =
            out.cfg.particle=="ion"
            ? out.cfg.ionQ * eplus
            : definition->GetPDGCharge();

        gun.SetParticleCharge(charge);

        // 这里仍然是整颗入射粒子的总动能，不是每核子动能。
        gun.SetParticleEnergy(out.cfg.energy * GeV);
        gun.SetParticleEnergy(out.cfg.energy*GeV);gun.SetParticleMomentumDirection({0,0,-1});
        gun.SetParticlePosition({x*m,y*m,z*m});gun.GeneratePrimaryVertex(event);
        out.primaries<<event->GetEventID()<<','<<charge / eplus<<','<<out.cfg.energy<<",1,0,"<<x<<','<<y<<','<<z<<'\n';
        if(event->GetEventID()%out.cfg.progress==0)G4cout<<"Primary event "<<event->GetEventID()<<G4endl;
    }
};
class Tracking:public G4UserTrackingAction {
    Output& out;
public:explicit Tracking(Output& o):out(o){}
    void PreUserTrackingAction(const G4Track* track) override {out.record(track);}
};

// 模块 7：命令行与生命周期。已有输出目录拒绝覆盖，失败返回非零状态。
// 不限定 tracking time，不截断中子/离子级联；高能尾事例耗时可能较大。
int main(int argc,char** argv){try{
    if(argc>2)throw std::runtime_error("Usage: muon_beam [config.mac]");
    if(argc==2&&std::string(argv[1])=="--help"){std::cout<<"Usage: muon_beam [config.mac]\n";return 0;}
    Config cfg=readCard(argc==2?argv[1]:"config.mac");
    gROOT->SetBatch(true);G4Random::setTheSeed(cfg.seed);
    Output output(cfg);
    std::ofstream saved(cfg.out+"/config_used.mac");saved<<cfg.cardText;saved.close();
    if(!saved)throw std::runtime_error("Cannot save configuration snapshot");
    auto manager=std::make_unique<G4RunManager>();manager->SetUserInitialization(new Detector(cfg));
    // HP 中子数据必须安装；参考物理表相比旧自定义表提供一致的 HP 物理组合。
    auto physics=new QGSP_BIC_HP(0);physics->ReplacePhysics(new G4EmStandardPhysics_option4(0));physics->SetDefaultCutValue(cfg.cutMM*mm);
    manager->SetUserInitialization(physics);manager->SetUserAction(new Gun(output));manager->SetUserAction(new Tracking(output));
    manager->Initialize();manager->BeamOn(cfg.events);output.finish();
    std::cout<<"Completed "<<cfg.events<<" primaries; output: "<<cfg.out<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}}
