// muon_stalactite_full.cc — 大气 μ 子入射圆柱矿物：完整输运 + ROOT 分析。
// 单线程设计：确保事件编号、CSV 和 ROOT 对象写入顺序明确，不引入线程共享输出。
// 构建/运行见同目录 README_muon_stalactite_full.md；STALACTITE_SELF_TEST 可独立测试抽样核心。
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

// 模块 1：用户给出的 GT 原式。E 是约定的动能 GeV，c=cos(theta)。
// 第三段严格按表达式替换幂律项；不要再按文字备注改回 E^(-gamma)。
double GT(double E,double c,double gamma=2.7) {
    if (!(E>0 && c>=0 && c<=1)) throw std::runtime_error("GT domain");
    constexpr double p1=.102573,p2=-.068287,p3=.958633,p4=.0407253,p5=.817285;
    const double cs=std::sqrt((c*c+p1*p1+p2*std::pow(c,p3)+p4*std::pow(c,p5))/(1+p1*p1+p2+p4));
    const double delta=2.06e-3*(950/cs-90);
    auto mesons=[&](double e){return 1/(1+1.1*e*cs/115)+.054/(1+1.1*e*cs/810);};
    if(E>=100/cs) return .14*std::pow(E,-gamma)*mesons(E);
    const double effective=E>=1/cs ? E : .3*E+.7/cs;
    const double survival=std::pow(90*std::sqrt(c+.001)/1030,4.5/(effective+delta)/cs);
    return .14*1.1*survival*std::pow(effective,-gamma)*(mesons(effective+delta)+1e-4);
}

// 模块 2：二维数值 CDF。坐标 u=ln(E)、c=cos(theta)，联合权重
// dPhi/(du dc)=2*pi*GT(exp(u),c)*c*exp(u)。最后的 E 是对数能量 Jacobian。
// theta 的 sin(theta) 已由 dc 吸收；不可额外再乘一次 sin(theta)。
// 单元中点积分并在单元内部均匀抽样，是明确的离散近似，非“精确公式抽样”。
struct Sampler {
    int ne,nc; double u0,du,dc,total=0; std::vector<double> cdf;
    // 构造器默认值只用于下面的独立回归测试。
    // 正式模拟总是显式传入 cfg.emin/cfg.emax，不使用这里的默认能窗。
    Sampler(int a=800,int b=400,double emin=.1,double emax=10000,double gamma=2.7):ne(a),nc(b),u0(std::log(emin)),du(std::log(emax/emin)/a),dc(1./b) {
        for(int i=0;i<ne;++i) for(int j=0;j<nc;++j) {
            double E=std::exp(u0+(i+.5)*du),c=(j+.5)*dc;
            total+=2*PI*GT(E,c,gamma)*c*E*du*dc; cdf.push_back(total);
        }
        if(!(total>0 && std::isfinite(total))) throw std::runtime_error("Invalid GT integral");
    }
    template<class Uniform> std::pair<double,double> sample(Uniform uniform) const {
        // 先按积分权重选二维单元，再在该单元的 ln(E)、cos(theta) 内均匀抽样。
        // 因而每个初级 μ 子的能量均处于卡片规定的闭包范围 [Emin,Emax]。
        auto k=std::lower_bound(cdf.begin(),cdf.end(),uniform()*total)-cdf.begin();
        k=std::min<std::ptrdiff_t>(k,cdf.size()-1);
        return {std::exp(u0+(k/nc+uniform())*du),(k%nc+uniform())*dc};
    }
};

// 自然时间：GT 为正负 μ 子总微分强度；total 已积分能量和投影立体角。
// 顶面面积从 m² 换算 cm²。1 年约定为 365.25 天，不再额外乘电荷比或 pi。
constexpr double SECONDS_PER_YEAR=365.25*24*3600;
// N/(面积*通量) 是入射事件数的期望曝光时间，不是 CPU 时间。
// flux 必须来自当前采样能窗；不能沿用其它能量阈值的通量。
double exposureSeconds(double events,double radiusM,double flux) {
    return events/(flux*PI*radiusM*radiusM*1e4);
}

#ifdef STALACTITE_SELF_TEST
// 独立单元测试：不用 Geant4/ROOT，可在普通 C++17 编译器运行。
int main(){
    auto check=[](bool ok){if(!ok) throw std::runtime_error("Self-test failed");};
    for(double c:{0.,.01,.5,1.}) for(double E:{.1,1.,10.,100.,10000.}) check(GT(E,c)>0&&std::isfinite(GT(E,c)));
    double expected=.14*std::pow(100.,-2.7)*(1/(1+1.1*100/115)+.054/(1+1.1*100/810));
    check(std::abs(GT(100,1)/expected-1)<1e-12);
    Sampler coarse(400,200),fine;
    check(std::abs(coarse.total/fine.total-1)<.005);
    // 验证反演 CDF 的采样均值与积分期望一致，而不只检查数值范围。
    double expectedC=0,expectedLow=0,previous=0;
    for(std::size_t k=0;k<fine.cdf.size();++k){
        double mass=fine.cdf[k]-previous;previous=fine.cdf[k];
        expectedC+=mass*((k%fine.nc+.5)*fine.dc);
        double left=fine.u0+(k/fine.nc)*fine.du;
        expectedLow+=mass*std::clamp((0.-left)/fine.du,0.,1.);
    }
    expectedC/=fine.total;expectedLow/=fine.total;
    std::mt19937_64 rng(20261031); std::uniform_real_distribution<double> u(0,1);
    double sum=0; int low=0;
    for(int i=0;i<100000;++i){auto [E,c]=fine.sample([&](){return u(rng);});check(E>=.1&&E<=10000&&c>=0&&c<=1);sum+=c;if(E<1)++low;}
    check(std::abs(sum/100000-expectedC)<.004);
    check(std::abs(low/100000.-expectedLow)<.008);
    // 两个低能分段在垂直方向的独立计算，覆盖生存因子及替换幂律。
    for(double E:{.2,10.}){
        double eff=E<1?.3*E+.7:E,delta=2.06e-3*(950-90);
        double ref=.154*std::pow(90*std::sqrt(1.001)/1030,4.5/(eff+delta))*std::pow(eff,-2.7)
                  *(1/(1+1.1*(eff+delta)/115)+.054/(1+1.1*(eff+delta)/810)+1e-4);
        check(std::abs(GT(E,1)/ref-1)<1e-12);
    }
    Sampler high(800,400,10,10000),highCoarse(400,200,10,10000);
    check(std::abs(highCoarse.total/high.total-1)<.005);
    double seconds=exposureSeconds(100000,10,high.total);
    check(std::abs(exposureSeconds(200000,10,high.total)/seconds-2)<1e-12);
    check(std::abs(exposureSeconds(100000,20,high.total)/seconds-.25)<1e-12);
    for(int i=0;i<10000;++i){auto [E,c]=high.sample([&](){return u(rng);});check(E>=10&&E<=10000);}
    std::cout<<std::setprecision(12)<<"10-10000 GeV flux="<<high.total<<"; 100000 events, radius 10 m: "<<seconds<<" s = "<<seconds/SECONDS_PER_YEAR<<" yr\n";
    std::cout<<"PASS: high-energy branch, positivity, domain, integral convergence\n"
             <<"Flux="<<fine.total<<" cm^-2 s^-1; coarse relative difference="<<coarse.total/fine.total-1
             <<"; sampled mean cos(theta)="<<sum/100000<<"; E<1 GeV="<<low<<"\n";
}
#else
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
#include "Randomize.hh"
#include "TFile.h"
#include "TH2D.h"
#include "TCanvas.h"
#include "TStyle.h"
#include "TColor.h"
#include "TROOT.h"

// 模块 3：集中配置。位置用米，记录阈值用 MeV；100000 是初级事件数，
// 每次事件仅发射一个 μ 子，并非核数。材料配置文件由项目 YAML 转换。
struct Config {
    std::string material="Halite",out="output_Halite",data="..",materials="materials.tsv";
    int events=100000; long seed=20261031;
    double radius=10,length=100,chargeRatio=1.3,thresholdMeV=.001;
    // 入射 μ 子下限必须由 mac 设置；0 是非法占位值，不是物理默认值。
    // 这不同于 thresholdMeV：后者只决定哪些产生核写入输出。
    double emin=0,emax=10000,gamma=2.7,cutMM=.005,rangeMin=.001,rangeMax=1e7;
    int energyBins=800,angleBins=400,xBins=80,zBins=100,rangeBins=100,progress=1000;
    int htmlRadialBins=10,htmlPhiBins=24,htmlZBins=50;
    std::string cardText;
};

// 配置卡是本程序的严格 /stalactite/ 命令集，初始化前一次性读取。
// 不交给 Geant4 UI：未知命令、单位、重复键和尾随字符立即报错，避免静默失效。
// 路径相对配置卡所在目录解析，支持引号；不依赖启动时的工作目录。
Config readCard(const std::string& filename) {
    Config cfg;
    const auto path=std::filesystem::absolute(filename);
    std::ifstream input(path);if(!input)throw std::runtime_error("Cannot open card: "+filename);
    std::map<std::string,bool> seen;std::string line;int number=0;
    while(std::getline(input,line)) {
        ++number;cfg.cardText+=line+"\n";
        auto hash=line.find('#');if(hash!=std::string::npos)line.resize(hash);
        std::istringstream row(line);std::string key,value,extra;
        if(!(row>>key))continue;
        if(!(row>>std::quoted(value))||(row>>extra))throw std::runtime_error("Bad card line "+std::to_string(number));
        if(seen[key])throw std::runtime_error("Duplicate command: "+key);seen[key]=true;
        auto real=[&](){std::size_t used;double v=std::stod(value,&used);if(used!=value.size()||!std::isfinite(v))throw std::runtime_error("Invalid number: "+key);return v;};
        auto integer=[&](){double v=real();if(v!=std::floor(v)||v<1||v>1000000000)throw std::runtime_error("Invalid integer: "+key);return static_cast<int>(v);};
        auto resolve=[&](){return (path.parent_path()/std::filesystem::path(value)).lexically_normal().string();};
        if(key=="/stalactite/material"){cfg.material=value;}
        else if(key=="/stalactite/output"){cfg.out=resolve();}
        else if(key=="/stalactite/data"){cfg.data=resolve();}
        else if(key=="/stalactite/materials"){cfg.materials=resolve();}
        else if(key=="/stalactite/events"){cfg.events=integer();}
        else if(key=="/stalactite/seed"){cfg.seed=integer();}
        else if(key=="/stalactite/radius_m"){cfg.radius=real();}
        else if(key=="/stalactite/length_m"){cfg.length=real();}
        else if(key=="/stalactite/muPlusOverMinus"){cfg.chargeRatio=real();}
        else if(key=="/stalactite/recordThreshold_MeV"){cfg.thresholdMeV=real();}
        // 入射 μ 子动能下限，GeV。该数值同时传入抽样网格和通量积分。
        else if(key=="/stalactite/energyMin_GeV"){cfg.emin=real();}
        else if(key=="/stalactite/energyMax_GeV"){cfg.emax=real();}
        else if(key=="/stalactite/gamma"){cfg.gamma=real();}
        else if(key=="/stalactite/productionCut_mm"){cfg.cutMM=real();}
        else if(key=="/stalactite/rangeMin_nm"){cfg.rangeMin=real();}
        else if(key=="/stalactite/rangeMax_nm"){cfg.rangeMax=real();}
        else if(key=="/stalactite/energyBins"){cfg.energyBins=integer();}
        else if(key=="/stalactite/angleBins"){cfg.angleBins=integer();}
        else if(key=="/stalactite/xBins"){cfg.xBins=integer();}
        else if(key=="/stalactite/zBins"){cfg.zBins=integer();}
        else if(key=="/stalactite/rangeBins"){cfg.rangeBins=integer();}
        else if(key=="/stalactite/htmlRadialBins"){cfg.htmlRadialBins=integer();}
        else if(key=="/stalactite/htmlPhiBins"){cfg.htmlPhiBins=integer();}
        else if(key=="/stalactite/htmlZBins"){cfg.htmlZBins=integer();}
        else if(key=="/stalactite/progressEvery"){cfg.progress=integer();}
        else throw std::runtime_error("Unknown card command: "+key);
    }
    // 不允许漏写下限后悄悄使用源码常量，避免不同运行的能窗不明确。
    if(!seen["/stalactite/energyMin_GeV"])
        throw std::runtime_error("Required card command: /stalactite/energyMin_GeV (GeV)");
    // 必须满足 0 < Emin < Emax；无效配置在 Geant4 初始化之前终止。
    if(cfg.radius<=0||cfg.length<=0||cfg.chargeRatio<0||cfg.thresholdMeV<0||cfg.emin<=0||cfg.emax<=cfg.emin||cfg.gamma<=0||cfg.cutMM<=0||cfg.rangeMin<=0||cfg.rangeMax<=cfg.rangeMin)
        throw std::runtime_error("Invalid physical limits in card");
    if(1LL*cfg.htmlRadialBins*cfg.htmlPhiBins*cfg.htmlZBins>100000)throw std::runtime_error("HTML grid exceeds 100000 voxels");
    if(1LL*cfg.energyBins*cfg.angleBins>10000000 || 1LL*cfg.xBins*cfg.zBins>1000000 || 1LL*cfg.zBins*cfg.rangeBins>1000000)
        throw std::runtime_error("Grid too large; reduce bin counts");
    // 路径项要求明确填写，避免默认相对路径被不同启动目录解释。
    for(auto k:{"output","data","materials"})if(!seen[std::string("/stalactite/")+k])throw std::runtime_error(std::string("Required card command: ")+k);
    return cfg;
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
        // 世界为真空：GT 已描述到达观测面的 μ 子，不重复模拟大气输运。
        auto vacuum=G4NistManager::Instance()->FindOrBuildMaterial("G4_Galactic");
        double half=std::max(cfg.length,cfg.radius)*1.2*m;
        auto world=new G4LogicalVolume(new G4Box("World",half,half,half),vacuum,"World");
        auto physical=new G4PVPlacement(nullptr,{},world,"World",nullptr,false,0,true);
        auto target=new G4LogicalVolume(new G4Tubs("Target",0,cfg.radius*m,cfg.length*m/2,0,twopi),loadMaterial(cfg),"Target");
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
    void finish(const Sampler& sampler){
        nuclei.flush();primaries.flush();if(!nuclei||!primaries)throw std::runtime_error("CSV write failed");
        TFile file((cfg.out+"/muon_stalactite_full.root").c_str(),"RECREATE");if(file.IsZombie())throw std::runtime_error("ROOT open failed");
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
        // 通量单位来自 GT 系数；顶面面积 m² -> cm²。时间仅是当前有限能窗/顶面定义下的等效曝光。
        std::ofstream meta(cfg.out+"/metadata.txt");
        meta<<std::setprecision(15)<<"Geant4="<<G4Version<<"\nmaterial="<<cfg.material<<"\nevents="<<cfg.events<<"\nseed="<<cfg.seed
            <<"\nradius_m="<<cfg.radius<<"\nlength_m="<<cfg.length<<"\nenergyMin_GeV="<<cfg.emin<<"\nenergyMax_GeV="<<cfg.emax<<"\nenergy_definition=kinetic\ngamma="<<cfg.gamma<<"\nmu_plus_over_minus="<<cfg.chargeRatio
            <<"\nGT_horizontal_flux_cm-2_s-1="<<sampler.total<<"\nequivalent_top_exposure_s="<<exposureSeconds(cfg.events,cfg.radius,sampler.total)
            <<"\nequivalent_top_exposure_years="<<exposureSeconds(cfg.events,cfg.radius,sampler.total)/SECONDS_PER_YEAR<<"\nseconds_per_year="<<SECONDS_PER_YEAR
            <<"\nsampler=midpoint logE/cosTheta joint CDF\nenergyBins="<<cfg.energyBins<<"\nangleBins="<<cfg.angleBins<<"\nphysics=QGSP_BIC_HP + EM option4\nproduction_cut_mm="<<cfg.cutMM<<"\nrecord_threshold_MeV="<<cfg.thresholdMeV
            <<"\nrange_source="<<cfg.data<<'/'<<cfg.material<<"/Range\nrange=linear interpolation of Range_um, no extrapolation\n"
            <<"scope=top-face incident only; full cascade transport; born ions include reaction products, not only elastic recoils\n";
        if(!summary||!meta)throw std::runtime_error("Metadata write failed");
    }
};

// 模块 6：入射枪。sqrt(U) 使圆盘面积均匀；在顶面内侧极小距离初始化，
// 避免接近水平的粒子从上方传播时发生横向位移，破坏顶面均匀条件。
class Gun:public G4VUserPrimaryGeneratorAction {
    Output& out;const Sampler& sampler;G4ParticleGun gun{1};
public:
    Gun(Output& o,const Sampler& s):out(o),sampler(s){}
    void GeneratePrimaries(G4Event* event) override {
        auto uniform=[](){return G4UniformRand();};auto [E,c]=sampler.sample(uniform);
        double phi=2*PI*uniform(),az=2*PI*uniform(),r=out.cfg.radius*std::sqrt(uniform());
        int charge=uniform()<out.cfg.chargeRatio/(1+out.cfg.chargeRatio)?1:-1;
        gun.SetParticleDefinition(G4ParticleTable::GetParticleTable()->FindParticle(charge>0?"mu+":"mu-"));
        gun.SetParticleEnergy(E*GeV);gun.SetParticleMomentumDirection({std::sqrt(1-c*c)*std::cos(phi),std::sqrt(1-c*c)*std::sin(phi),-c});
        double z=out.cfg.length/2-1e-9;
        gun.SetParticlePosition({r*std::cos(az)*m,r*std::sin(az)*m,z*m});gun.GeneratePrimaryVertex(event);
        out.primaries<<event->GetEventID()<<','<<charge<<','<<E<<','<<c<<','<<phi<<','<<r*std::cos(az)<<','<<r*std::sin(az)<<','<<z<<'\n';
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
    if(argc>2)throw std::runtime_error("Usage: muon_stalactite_full [config.mac]");
    if(argc==2&&std::string(argv[1])=="--help"){std::cout<<"Usage: muon_stalactite_full [config.mac]\n";return 0;}
    Config cfg=readCard(argc==2?argv[1]:"config.mac");
    gROOT->SetBatch(true);G4Random::setTheSeed(cfg.seed);
    // 唯一正式抽样入口：从 mac 读取的能量下限直接生效，不二次覆盖。
    // sampler.total 也对同一能窗积分，修改阈值会自动改变自然曝光时间。
    Sampler sampler(cfg.energyBins,cfg.angleBins,cfg.emin,cfg.emax,cfg.gamma);Output output(cfg);
    const double naturalSeconds=exposureSeconds(cfg.events,cfg.radius,sampler.total);
    // 开始输运前打印，长任务不必等运行结束才知道计划曝光时间。
    std::cout<<std::setprecision(12)<<"GT energy window: "<<cfg.emin<<".."<<cfg.emax<<" GeV\n"
             <<"Horizontal muon flux: "<<sampler.total<<" cm^-2 s^-1\n"
             <<"Planned natural exposure ("<<cfg.events<<" primaries, top face only): "
             <<naturalSeconds/SECONDS_PER_YEAR<<" years ("<<naturalSeconds<<" seconds)\n"<<std::flush;
    std::ofstream exposure(cfg.out+"/planned_exposure.txt");
    exposure<<std::setprecision(15)<<"planned_events="<<cfg.events<<"\nenergyMin_GeV="<<cfg.emin<<"\nenergyMax_GeV="<<cfg.emax
            <<"\nradius_m="<<cfg.radius<<"\nGT_horizontal_flux_cm-2_s-1="<<sampler.total
            <<"\nplanned_exposure_seconds="<<naturalSeconds<<"\nplanned_exposure_years="<<naturalSeconds/SECONDS_PER_YEAR
            <<"\nseconds_per_year="<<SECONDS_PER_YEAR<<"\nstatus=planned; completion requires successful run end\n";
    exposure.close();if(!exposure)throw std::runtime_error("Cannot save planned exposure");
    std::ofstream saved(cfg.out+"/config_used.mac");saved<<cfg.cardText;saved.close();
    if(!saved)throw std::runtime_error("Cannot save configuration snapshot");
    auto manager=std::make_unique<G4RunManager>();manager->SetUserInitialization(new Detector(cfg));
    // HP 中子数据必须安装；参考物理表相比旧自定义表提供一致的 HP 物理组合。
    auto physics=new QGSP_BIC_HP(0);physics->ReplacePhysics(new G4EmStandardPhysics_option4(0));physics->SetDefaultCutValue(cfg.cutMM*mm);
    manager->SetUserInitialization(physics);manager->SetUserAction(new Gun(output,sampler));manager->SetUserAction(new Tracking(output));
    manager->Initialize();manager->BeamOn(cfg.events);output.finish(sampler);
    std::cout<<"Completed "<<cfg.events<<" primaries; output: "<<cfg.out<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}}
#endif
