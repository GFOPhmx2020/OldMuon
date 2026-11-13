// ============================================================================
// 程序作用
// ============================================================================
// 模拟流程：
//   1. PYTHIA 生成电子反中微子与电子相互作用产生的 W，以及 W 的强子末态。
//   2. 将 PYTHIA 质心系末态转换到电子静止的实验室系。
//   3. 将这些末态粒子交给 Geant4，在 NaCl 盐靶中传播。
//   4. 记录满足条件的次级核的出生信息和轨迹终点信息。
//
// 输出是“每个已经发生的 W 强子事件”的条件响应。
// 本程序没有乘入宇宙中微子通量、靶电子总数、暴露时间或地质年龄。
// 本程序也没有模拟晶格损伤的长期保存、退火和实验可见径迹形成过程。
//
// 注意本文件存在三个不同的阈值概念：
//   A. secondaryMinMeV：次级粒子出生后的跟踪筛选，默认 10000 MeV。
//   B. recordBirth 中的 10*GeV：是否写入核出生表的筛选。
//   C. SetDefaultCutValue(0.005*mm)：物理表的生产长度截断。
// 三者不是同一个参数，不能相互替代。

#include "OutputFile.hh"    // 本项目封装：普通 TSV / gzip 压缩 TSV 输出。
#include "Pythia8/Pythia.h" // PYTHIA 事件生成器。

// Geant4 物理表及其配置。
#include "FTFP_BERT_HP.hh"
#include "G4EmStandardPhysics_option4.hh"
#include "G4EmParameters.hh"
#include "G4HadronicParameters.hh"
#include "G4NeutronTrackingCut.hh"

// Geant4 运行和事件管理。
#include "G4RunManager.hh"
#include "G4Run.hh"
#include "G4Event.hh"

// 用户提供的几何构建、初级粒子生成，以及事件/堆栈/跟踪回调。
#include "G4VUserDetectorConstruction.hh"
#include "G4VUserPrimaryGeneratorAction.hh"
#include "G4UserEventAction.hh"
#include "G4UserStackingAction.hh"
#include "G4UserTrackingAction.hh"
#include "G4UserSteppingAction.hh"
#include "G4ProductionCutsTable.hh"

// 跟踪管理器及附加在轨迹上的自定义信息。
#include "G4TrackingManager.hh"
#include "G4VUserTrackInformation.hh"

// Geant4 初级顶点和初级粒子。
#include "G4PrimaryVertex.hh"
#include "G4PrimaryParticle.hh"

// 粒子定义、材料及几何。
#include "G4ParticleTable.hh"
#include "G4ParticleDefinition.hh"
#include "G4NistManager.hh"
#include "G4Material.hh"
#include "G4Box.hh"
#include "G4Tubs.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"

// 轨迹、步、物理过程、单位、版本及随机数。
#include "G4Track.hh"
#include "G4Step.hh"
#include "G4VProcess.hh"
#include "G4SystemOfUnits.hh"
#include "G4Version.hh"
#include "Randomize.hh"

// C++ 标准库。
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>

// 给 std::filesystem 起一个简短别名，方便操作目录和路径。
namespace fs = std::filesystem;


// ============================================================================
// 1. 运行配置
// ============================================================================
// 这里只存参数，不进行实际模拟。
// 命令行中的能量使用 GeV，几何尺寸使用米。
// 送入 Geant4 的量通过 *GeV、*m 等显式转换到 Geant4 内部单位。
struct Config {
    double energyGeV;  // 入射电子反中微子的实验室系能量。
    int events;        // 要生成/输运的 W 强子事件数。
    long seed;         // 随机数种子，用于重复指定配置下的模拟。
    fs::path output;   // 输出目录，要求是尚不存在的新目录。

    double radiusM = 10.; // 圆柱盐靶半径，单位 m。
    double lengthM = 20.; // 圆柱盐靶全长，单位 m，不是半长。

    // true：只生成 PYTHIA 末态，不进行 Geant4 材料输运。
    bool generateOnly = false;

    // true：输出 .tsv.gz；false：输出 .tsv。
    // 压缩只改变存储形式，不改变模拟物理。
    bool compress = true;

    // 所有 Geant4 次级粒子的出生动能筛选阈值，单位 MeV。
    // 当前代码默认 1000 MeV = 1 GeV，不是此前的 1 MeV。
    // 可由 GLASHOW_SECONDARY_MIN_MEV 环境变量覆盖。
    // 设为 0 关闭此处的出生截断，但不会关闭 recordBirth 中的记录阈值。
    double secondaryMinMeV = 1.;
    // 核出生记录的最低动能，单位 MeV。
    double recoilRecordMinMeV = 1.;
};


// 记录直接母粒子的身份。
// pdg：PDG 粒子编号；z：核电荷数；a：核质量数。
// 这里不保存完整祖先链，也不追溯到最初是哪一个 PYTHIA 粒子。
struct Parent { int pdg, z, a; };


// ============================================================================
// 2. 附加在核轨迹上的信息
// ============================================================================
// final：这个类不再被其他类继承。
// G4VUserTrackInformation：Geant4 允许用户给每条轨迹附加的信息基类。
//
// 仅核轨迹携带本对象；轨迹删除时由 Geant4 释放。
// 避免用一个永久增长的全局字典保存所有粒子的谱系。
struct RecoilInfo final: G4VUserTrackInformation {
    Parent parent;          // 直接母粒子的 PDG、Z、A。
    bool recorded = false;  // 该核轨迹是否已经写入出生表。
    // 区分人为动能截断与正常停止、核反应等终止方式。
    bool killedByEnergyCut = false;
    // static：所有 RecoilInfo 对象共享这两个计数器。
    // live：当前仍存在的对象数；peak：整个运行中的最大同时存在数。
    inline static std::uint64_t live = 0, peak = 0;

    // 创建一个对象时增加 live，并更新峰值。
    explicit RecoilInfo(Parent p): parent(p) {
        peak = std::max(peak, ++live);
    }

    // 对象销毁时减少 live。
    ~RecoilInfo() override { --live; }
};


// ============================================================================
// 3. 各模块共享的运行状态和输出文件
// ============================================================================
struct State {
    Config cfg; // 保存本次运行配置的一份副本。

    // recoils：核出生表。
    // endings：核轨迹终点表。
    // events：完整结束的事件账本。
    // primaries：PYTHIA 转换到实验室系后的初级粒子表。
    OutputFile recoils, endings, events, primaries;

    // 整个运行累计的出生截断统计，不在每个事件开始时清零。
    // 注意：这里累加的是被删除粒子的动能，不包括其静质量能。
    // 它只是诊断量，不等于沉积到材料中的能量。
    std::uint64_t killedSecondaries = 0;
    double killedKineticMeV = 0.;

    // 当前事件的核出生记录数、跟踪回调访问数。
    std::uint64_t recordedCount = 0, trackVisits = 0;

    // 使用单调时钟控制进度打印，避免系统时间调整影响间隔判断。
    std::chrono::steady_clock::time_point lastProgress =
        std::chrono::steady_clock::now();

    int eventID = -1; // 当前事件编号，Geant4 通常从 0 开始。
    int completed = 0; // 已完整结束输运的事件数。
    int generated = 0; // 已成功生成并交付初级粒子的事件数。
    int nPrimary = 0;  // 当前事件交给 Geant4 的初级粒子数。

    // 当前事件全部初级粒子的总能量、总纵向动量，单位 GeV。
    double sumEnergy = 0., sumPz = 0.;

    // 从 PYTHIA 粒子表读取，单位 GeV。
    double electronMass = 0., wMass = 0., wWidth = 0.;

    // 生成器粒子质量与 Geant4 粒子表质量差异的最大绝对值。
    double maxMassMismatchGeV = 0.;

    explicit State(const Config& c): cfg(c) {
        // 目录已存在时拒绝继续，防止覆盖之前的模拟结果。
        // 目录创建本身发生权限或磁盘错误时，也会抛出异常。
        if (!fs::create_directories(c.output))
            throw std::runtime_error(
                "Output directory already exists; use a NEW output name.");

        open(recoils, "recoils.tsv");
        open(endings, "recoil_endings.tsv");
        open(events, "events.tsv");
        open(primaries, "primaries.tsv");

        // TSV：各列用制表符 \t 分隔，每条记录以 \n 结束。
        //
        // 核出生表：
        // energy_mev 是出生动能，不是总能量。
        // ux、uy、uz 是单位方向矢量的分量。
        // creator 是产生该核轨迹的过程名称。
        recoils << "event_id\ttrack_id\tparent_id\tpdg\tz\ta\tenergy_mev"
                   "\tx_mm\ty_mm\tz_mm\tux\tuy\tuz\ttime_ns\tcreator"
                   "\tparent_pdg\tparent_z\tparent_a\n";

        // 核终点表：
        // path_length_mm 是整条轨迹的累计路程，不是起终点直线距离；
        // 此处也没有将它限定为盐靶内部的路程。
        endings << "event_id\ttrack_id\tfinal_energy_mev\tpath_length_mm"
                   "\tx_mm\ty_mm\tz_mm\tend_process\tstatus\n";

        // 完整事件账本：即使没有记录到核，该事件仍应写入。
        events << "event_id\tenergy_gev\tn_primaries\tn_recorded_nuclei"
                  "\tsum_primary_energy_gev\tsum_primary_pz_gev\n";

        // 初级粒子表中的 energy_gev 是总能量，包含静质量能。
        primaries << "event_id\tindex\tpdg\tpx_gev\tpy_gev\tpz_gev\tenergy_gev\n";
    }

    void open(OutputFile& f, const char* filename) {
        // 按压缩配置自动添加 .gz 后缀。
        f.open(
            cfg.output /
                (std::string(filename) + (cfg.compress ? ".gz" : "")),
            cfg.compress);

        // 保存较高有效数字精度，便于比较、守恒检查和回归检查。
        f << std::setprecision(17);
    }

    void closeOutputs() {
        // 正常退出前关闭全部文件，完成压缩流并检查写入错误。
        // 主程序在这些操作成功后才将 complete 标记为 true。
        recoils.close();
        endings.close();
        events.close();
        primaries.close();
    }

    void metadata(bool complete) {
        // 将本次配置和累计状态重新写入 run.json。
        // complete=false 表示本次调用尚未声明整个任务正常完成。
        // complete=true 也需结合 generate_only 判断是否真正做过输运。
        //
        // 这里是直接覆盖写入，不是原子替换。
        // 若进程恰好在写 JSON 时被终止，JSON 也可能不完整。
        std::ofstream f;

        // 输出失败时抛出异常，而不是静默忽略磁盘写入错误。
        f.exceptions(std::ios::failbit | std::ios::badbit);
        f.open(cfg.output / "run.json");
        f << std::setprecision(17);

        // 核记录阈值为 1 GeV；JSON 使用 keV 表示，即 1000000 keV。
// 此值应与 recordBirth() 中的记录阈值保持一致。
        f << "{\n  \"schema\": 1,\n  \"complete\": "
          << (complete ? "true" : "false")
          << ",\n  \"energy_gev\": " << cfg.energyGeV
          << ",\n  \"requested_events\": " << cfg.events
          << ",\n  \"completed_events\": " << completed
          << ",\n  \"generated_events\": " << generated
          << ",\n  \"secondary_min_kinetic_mev\": " << cfg.secondaryMinMeV
          << ",\n  \"killed_secondaries\": " << killedSecondaries
          << ",\n  \"killed_secondary_kinetic_mev\": " << killedKineticMeV
          << ",\n  \"output_compression\": \""
          << (cfg.compress ? "gzip" : "none") << "\""
          << ",\n  \"genealogy_storage\": \"live nuclear tracks only\""
          << ",\n  \"peak_live_recoil_info\": " << RecoilInfo::peak
          << ",\n  \"generate_only\": " << (cfg.generateOnly ? "true" : "false")
          << ",\n  \"seed\": " << cfg.seed
          << ",\n  \"radius_m\": " << cfg.radiusM
          << ",\n  \"length_m\": " << cfg.lengthM
          << ",\n  \"density_g_cm3\": 2.16"
          << ",\n  \"record_threshold_keV\": "
          << cfg.recoilRecordMinMeV * 1000.
          << ",\n  \"electron_mass_gev\": " << electronMass
          << ",\n  \"w_mass_gev\": " << wMass
          << ",\n  \"w_width_gev\": " << wWidth
          << ",\n  \"max_primary_mass_difference_gev\": "
          << maxMassMismatchGeV
          << ",\n  \"tracking_energy_cut_enabled\": true"
          << ",\n  \"tracking_min_kinetic_mev\": " << cfg.secondaryMinMeV
          << ",\n  \"geant4\": \"" << G4Version << "\""
          << ",\n  \"pythia_version\": " << PYTHIA_VERSION
          << ",\n  \"physics\": \"FTFP_BERT_HP + EM option4, upper energy 20 PeV\""
          << ",\n  \"neutron_time_limit_ns\": 1000000"
          << ",\n  \"conditional_hadronic_W\": true"
          << ",\n  \"initial_state_radiation\": false"
          << ",\n  \"vertex\": \"salt cylinder centre, neutrino along +z\""
          << ",\n  \"limitations\": \"free stationary electron; PeV models unvalidated; "
             "Geant4 heavy-flavour decay approximation; range proxy is not preserved damage\"\n}\n";
    }
};


// ============================================================================
// 4. 次级粒子出生筛选
// ============================================================================
// Geant4 的物理模型已经生成次级粒子后，本回调决定是否让它进入跟踪。
// 因此这里减少的是后续输运工作，不是禁止相互作用模型产生低能粒子。
//
// 筛选作用于所有次级粒子，包括电子、光子、中子、核。
// 不限制出生体积，不要求粒子位于盐靶内。
// 低于阈值的核在调用出生记录函数之前就被删除，不进入核输出表。
class SecondaryCut final: public G4UserStackingAction {
    State& s; // 引用共享状态，统计结果直接更新到 State 中。

public:
    explicit SecondaryCut(State& state): s(state) {}

    G4ClassificationOfNewTrack ClassifyNewTrack(const G4Track* t) override {
        // ParentID==0：Geant4 初级粒子，不受本筛选影响。
        // ParentID>0：其他粒子在输运中产生的次级粒子。
        //
        // 严格使用“小于”：恰好等于阈值的粒子会被保留。
        if (t->GetParentID()>0 &&
            t->GetKineticEnergy()<s.cfg.secondaryMinMeV*MeV) {
            ++s.killedSecondaries;
            s.killedKineticMeV += t->GetKineticEnergy()/MeV;

            // 删除轨迹，不自动把其能量沉积到材料。
            return fKill;
        }

        // 放入正常待跟踪队列。
        // 此处不是每步动能限制：已通过筛选的粒子减速后仍继续跟踪。
        return fUrgent;
    }
};
// 每一步结束后检查动能：低于阈值时终止当前轨迹。
class TrackingEnergyCut final : public G4UserSteppingAction {
    State& s;

public:
    explicit TrackingEnergyCut(State& state) : s(state) {}

    void UserSteppingAction(const G4Step* step) override {
        auto* track = step->GetTrack();

        // 已由其他过程终止的轨迹不再处理。
        const auto status = track->GetTrackStatus();
        if (status != fAlive && status != fStopButAlive) return;

        // 与出生截断共用配置，避免两处阈值不同。
        // secondaryMinMeV=1000 时，对应 1 GeV。
        // 设为 0 时关闭本回调的动能截断。
        const double cut = s.cfg.secondaryMinMeV * MeV;
        if (cut <= 0.) return;

        // 此处读取的是当前步结束后的动能。
        // 恰好等于阈值时保留，严格低于阈值时终止。
        if (track->GetKineticEnergy() >= cut) return;

        // 如果是带有谱系信息的核，标记其人为终止原因。
        if (auto* info =
                dynamic_cast<RecoilInfo*>(track->GetUserInformation())) {
            info->killedByEnergyCut = true;
        }

        // 只终止当前轨迹。
        // 本步已经产生的次级粒子仍交给现有 SecondaryCut 筛选。
        track->SetTrackStatus(fStopAndKill);
    }
};

// ============================================================================
// 5. 材料与几何
// ============================================================================
// 几何由外部真空世界和中心圆柱盐靶组成。
// 本项目通过物理体积名称 "Target" 判断核是否在盐靶内出生。
class Detector final: public G4VUserDetectorConstruction {
    Config cfg;

public:
    explicit Detector(const Config& c): cfg(c) {}

    G4VPhysicalVolume* Construct() override {
        auto* nist = G4NistManager::Instance();

        // 定义密度为 2.16 g/cm^3、包含两种元素的盐材料。
        auto* salt = new G4Material("GlashowHalite", 2.16*g/cm3, 2);

        // 第二个参数为整数，表示化学计量的原子个数。
        // Na:Cl=1:1 是原子数比，不是质量比。
        salt->AddElement(nist->FindOrBuildElement("Na"), 1);
        salt->AddElement(nist->FindOrBuildElement("Cl"), 1);

        // 世界盒子的半边长：保证能包住盐靶，并留出外部真空空间。
        const double worldHalf =
            1.2*std::max(cfg.lengthM/2., cfg.radiusM)*m + m;

        // Solid 定义形状；LogicalVolume 将形状与材料绑定。
        auto* wlv = new G4LogicalVolume(
            new G4Box("World", worldHalf, worldHalf, worldHalf),
            nist->FindOrBuildMaterial("G4_Galactic"),
            "World");

        // PhysicalVolume 将逻辑体放入实际空间。
        // 世界体没有母体积，因此母体积参数为 nullptr。
        auto* world =
            new G4PVPlacement(nullptr, {}, wlv, "World", nullptr, false, 0, true);

        // G4Tubs 参数：内半径、外半径、z 半长、起始角、张角。
        // 内半径为 0，表示实心圆柱；圆柱轴沿 z。
        auto* target = new G4LogicalVolume(
            new G4Tubs(
                "Target",
                0.,
                cfg.radiusM*m,
                cfg.lengthM*m/2.,
                0.,
                360.*deg),
            salt,
            "Target");

        // 盐靶与世界中心重合；无旋转、无位移。
        // 最后一个 true 请求放置时执行重叠检查。
        new G4PVPlacement(nullptr, {}, target, "Target", wlv, false, 0, true);

        return world;
    }
};


// ============================================================================
// 6. PYTHIA 生成器及实验室系转换
// ============================================================================
// Geant4 不在本程序中直接生成 Glashow 共振。
// PYTHIA 生成的末态被作为 Geant4 初级粒子，再由 Geant4 负责材料输运。
class Generator final: public G4VUserPrimaryGeneratorAction {
    State& s;
    Pythia8::Pythia p;

    // 从质心系转换到实验室系所需的 gamma 和 gamma*beta。
    double gamma, gammaBeta;

    void setting(const std::string& text) {
        // 检查设置字符串是否被 PYTHIA 接受。
        if (!p.readString(text))
            throw std::runtime_error("Invalid PYTHIA setting: " + text);
    }

public:
    explicit Generator(State& state): s(state) {
        s.electronMass = p.particleData.m0(11);
        s.wMass = p.particleData.m0(24);
        s.wWidth = p.particleData.mWidth(24);

        // 静止电子靶近似下：
        // s = me^2 + 2*me*Eν；eCM = sqrt(s)。
        // 此处忽略中微子质量，也不包含原子束缚电子运动。
        const double eCM =
            std::sqrt(s.electronMass*s.electronMass +
                      2*s.electronMass*s.cfg.energyGeV);

        // 实验室系总能量为 Eν+me，总纵向动量约为 Eν。
        gamma = (s.cfg.energyGeV+s.electronMass)/eCM;
        gammaBeta = s.cfg.energyGeV/eCM;

        // PDG -12：电子反中微子；PDG 11：电子。
        setting("Beams:idA = -12");
        setting("Beams:idB = 11");

        // 在质心系按指定 eCM 生成事件，之后手动 boost。
        setting("Beams:frameType = 1");
        p.settings.parm("Beams:eCM", eCM);

        setting("PDF:lepton = off");
        setting("PDF:neutrino = off");
        setting("WeakSingleBoson:ffbar2W = on");

        // 原代码设置保留：
        // 先关闭 W 衰变道，再开启包含 d/u/s/c/b 夸克编号的衰变道。
        // 得到的是条件强子事件样本，不在这里乘强子分支比。
        setting("24:onMode = off");
        setting("24:onIfAny = 1 2 3 4 5");

        // 关闭初态辐射。
        setting("PartonLevel:ISR = off");

        // 只让固有寿命对应的 c*tau 足够短的粒子在 PYTHIA 中衰变。
        // tau0Max 的单位是 mm；更长寿命粒子的后续处理交给 Geant4。
        setting("ParticleDecays:limitTau0 = on");

        // shortcut: transport long-lived hadrons with G4's decay tables;
        // replace/validate heavy-flavour decays before physics interpretation.
        setting("ParticleDecays:tau0Max = 1e-9");

        // PYTHIA 与 Geant4 分别设置各自随机数系统。
        setting("Random:setSeed = on");

        // 关闭大量逐事件打印，完整初级粒子信息仍写入 primaries 表。
        setting("Next:numberShowInfo = 0");
        setting("Next:numberShowProcess = 0");
        setting("Next:numberShowEvent = 0");

        p.settings.mode("Random:seed", static_cast<int>(s.cfg.seed));

        if (!p.init())
            throw std::runtime_error("PYTHIA initialization failed");

        // 保存生成器设置，便于日后追溯本次运行配置。
        p.settings.writeFile((s.cfg.output / "pythia_settings.cmnd").string());
        s.metadata(false);
    }

    void GeneratePrimaries(G4Event* event) override {
        // 允许生成失败后重试，但连续 100 次失败则停止。
        bool ok = false;
        for (int attempt=0; attempt<100; ++attempt) {
            if (p.next()) {
                ok=true;
                break;
            }
        }

        if (!ok)
            throw std::runtime_error("PYTHIA failed 100 consecutive attempts");

        // 检查硬过程记录里确实存在 W-。
        bool foundW = false;
        for (int j=0; j<p.process.size(); ++j)
            if (p.process[j].id()==-24) foundW=true;

        if (!foundW)
            throw std::runtime_error("Generated process has no W-minus");

        // 清零本事件的初级粒子统计。
        s.eventID = event->GetEventID();
        s.nPrimary = 0;
        s.sumEnergy = 0.;
        s.sumPz = 0.;
        double pxSum=0., pySum=0., charge=0.;

        // 所有初级粒子从盐靶中心、t=0 出发。
        // 本程序没有单独输运 W 的飞行轨迹或其衰变顶点分布。
        auto vertex =
            std::make_unique<G4PrimaryVertex>(G4ThreeVector(0,0,0), 0.);

        for (int j=0; j<p.event.size(); ++j) {
            const auto& q = p.event[j];

            // 只传递 PYTHIA 事件记录中的最终态条目。
            // PYTHIA 中未衰变的长寿命粒子也可以属于这里的最终态。
            if (!q.isFinal()) continue;

            auto* def =
                G4ParticleTable::GetParticleTable()->FindParticle(q.id());

            if (!def)
                throw std::runtime_error(
                    "Missing Geant4 particle PDG " + std::to_string(q.id()));

            // 沿 +z 做洛伦兹变换：
            // pz_lab = gamma*pz_CM + gamma*beta*E_CM
            // E_lab  = gamma*E_CM  + gamma*beta*pz_CM
            // 横向 px、py 不变。PYTHIA 的这些量使用 GeV。
            const double pz = gamma*q.pz() + gammaBeta*q.e();
            const double energy = gamma*q.e() + gammaBeta*q.pz();

            if (!std::isfinite(energy) || energy<=0.)
                throw std::runtime_error("Invalid boosted primary");

            // 通过三动量构造 Geant4 初级粒子，并显式转换单位。
            auto* primary = new G4PrimaryParticle(
                def, q.px()*GeV, q.py()*GeV, pz*GeV);

            // 保留生成器给出的在壳质量。
            primary->SetMass(q.m()*GeV);

            s.maxMassMismatchGeV = std::max(
                s.maxMassMismatchGeV,
                std::abs(q.m()-def->GetPDGMass()/GeV));

            // 将粒子挂到本事件初级顶点上。
            vertex->SetPrimary(primary);

            s.primaries
                << s.eventID << '\t' << s.nPrimary << '\t' << q.id()
                << '\t' << q.px() << '\t' << q.py()
                << '\t' << pz << '\t' << energy << '\n';

            ++s.nPrimary;
            s.sumEnergy+=energy;
            s.sumPz+=pz;
            pxSum+=q.px();
            pySum+=q.py();
            charge+=q.charge();
        }

        // 检查生成器末态与初始 νbar_e + e- 的总四动量及电荷相符。
        // 总能量包含靶电子静质量；总电荷应为 -1。
        // 这是生成器/坐标变换检查，不是整场材料输运的能量守恒审计。
        const double tolerance =
            1e-6*(s.cfg.energyGeV+s.electronMass);

        if (std::abs(s.sumEnergy-s.cfg.energyGeV-s.electronMass)>tolerance ||
            std::abs(s.sumPz-s.cfg.energyGeV)>tolerance ||
            std::abs(pxSum)>1e-5 ||
            std::abs(pySum)>1e-5 ||
            std::abs(charge+1.)>1e-6)
            throw std::runtime_error(
                "Primary four-momentum/charge conservation failed");

        // release：将顶点所有权交给 Geant4 事件管理。
        event->AddPrimaryVertex(vertex.release());

        ++s.generated;
        s.primaries.flush();
        s.metadata(false);
    }
};


// ============================================================================
// 7. 每个事件开始和结束时的处理
// ============================================================================
class Events final: public G4UserEventAction {
    State& s;

public:
    explicit Events(State& state): s(state) {}

    void BeginOfEventAction(const G4Event*) override {
        // 只清零当前事件统计，不清零整个运行的截断累计值。
        s.recordedCount = 0;
        s.trackVisits = 0;
        s.lastProgress = std::chrono::steady_clock::now();
    }

    void EndOfEventAction(const G4Event* e) override {
        // 被 Geant4 中止的事件不能计入完整事件数。
        if (e->IsAborted())
            throw std::runtime_error(
                "Aborted event; output remains incomplete");

        // 无论记录到多少核，完整结束的 W 事件都要计入归一化分母。
        s.events
            << e->GetEventID() << '\t' << s.cfg.energyGeV
            << '\t' << s.nPrimary << '\t' << s.recordedCount
            << '\t' << s.sumEnergy << '\t' << s.sumPz << '\n';

        ++s.completed;

        G4cout
            << "Completed W event " << s.completed << "/" << s.cfg.events
            << ", recorded nuclei: " << s.recordedCount << G4endl;

        // 刷新已完成事件对应的数据。
        // 整个任务尚未最终关闭输出，所以此处仍写 complete=false。
        s.events.flush();
        s.recoils.flush();
        s.endings.flush();
        s.metadata(false);
    }
};


// ============================================================================
// 8. 核出生记录：判定“哪些核进入原始表”的核心函数
// ============================================================================
// 先前在 Geant4 11.2 中核对过：PreUserTrackingAction 前已完成初始定位。
// 这里读取尚未走第一步的轨迹状态，作为出生位置和出生动能。
//
// 一条符合条件的核轨迹写一行；不是每步写一行。
// 本函数不要求 creator 为弹性散射，因此记录可包含残核、碎片等。
void recordBirth(State& s, const G4Track* t) {
    // 排除已经终止、暂停等非当前接受状态的轨迹。
    if (t->GetTrackStatus()!=fAlive &&
        t->GetTrackStatus()!=fStopButAlive) return;

    // 排除初级粒子；排除已经走过步的轨迹。
    if (t->GetParentID()==0 ||
        t->GetCurrentStepNumber()!=0) return;

    const auto* d=t->GetDefinition();
    int z=d->GetAtomicNumber(), a=d->GetAtomicMass();

    // 仅保留 Z>=1、A>=2 的核。
    // 质子 A=1、中子、电子、光子不写入此表。
    if (z<1 || a<2) return;

    const auto* volume=t->GetVolume();

    // 必须在盐靶 Target 内出生，且出生动能至少为 1 GeV。
    //
    // 注意这是单独的“记录阈值”，不随 secondaryMinMeV 自动改变。
    // 在靶外出生、后来进入靶内的核不会因此补记；
    // 在靶内出生、后来逃出盐靶的核仍保留出生记录。
    if (!volume ||
        volume->GetName()!="Target" ||
        t->GetKineticEnergy()<s.cfg.recoilRecordMinMeV*MeV) return;

    auto* info=dynamic_cast<RecoilInfo*>(t->GetUserInformation());

    if (!info)
        throw std::runtime_error("Missing parent genealogy");

    if (info->recorded)
        throw std::runtime_error("Duplicate recoil birth");

    const auto pos=t->GetPosition(), dir=t->GetMomentumDirection();
    const auto* proc=t->GetCreatorProcess();

    // Geant4 内部单位转换为输出文件约定的 MeV、mm、ns。
    s.recoils
        << s.eventID << '\t' << t->GetTrackID() << '\t' << t->GetParentID()
        << '\t' << d->GetPDGEncoding() << '\t' << z << '\t' << a
        << '\t' << t->GetKineticEnergy()/MeV
        << '\t' << pos.x()/mm << '\t' << pos.y()/mm << '\t' << pos.z()/mm
        << '\t' << dir.x() << '\t' << dir.y() << '\t' << dir.z()
        << '\t' << t->GetGlobalTime()/ns
        << '\t' << (proc ? proc->GetProcessName() : "none")
        << '\t' << info->parent.pdg
        << '\t' << info->parent.z
        << '\t' << info->parent.a << '\n';

    info->recorded = true;
    ++s.recordedCount;
}


// ============================================================================
// 9. 轨迹开始/结束时的处理
// ============================================================================
// 本类承担三件事：
//   A. 在核开始跟踪时尝试记录出生。
//   B. 为新产生的核附加直接母粒子信息。
//   C. 为已经记录出生的核写入轨迹终点。
class Tracking final: public G4UserTrackingAction {
    State& s;

public:
    explicit Tracking(State& state): s(state) {}

    void PreUserTrackingAction(const G4Track* t) override {
        // 当前程序为核次级粒子附加 RecoilInfo；
        // 非核粒子没有这些信息，因此跳过核出生记录器。
        if (t->GetUserInformation()) recordBirth(s, t);

        // 先以每 100000 次回调为检查间隔，避免每条轨迹都查询时钟。
        if (++s.trackVisits % 100000 != 0) return;

        const auto now = std::chrono::steady_clock::now();

        // 只有距上次打印至少 30 秒，才真正输出进度。
        // 因此这是“满足计数条件时检查时间”，不是严格每 30 秒触发。
        if (now - s.lastProgress < std::chrono::seconds(30)) return;

        s.lastProgress = now;
        s.recoils.flush();
        s.endings.flush();

        G4cout
            << "Progress event " << s.eventID
            << ": tracked=" << s.trackVisits
            << ", recorded nuclei=" << s.recordedCount
            << ", live genealogy=" << RecoilInfo::live << G4endl;
    }

    void PostUserTrackingAction(const G4Track* t) override {
        // 当前母轨迹的这次跟踪结束后，访问它产生的次级粒子。
        // 在这些核开始自己的跟踪之前，为它们附加母粒子身份。
        if (auto* children = fpTrackingManager->GimmeSecondaries()) {
        // 母粒子的身份对所有直接子粒子相同，只读取一次。
        const auto* d = t->GetDefinition();
        const Parent parent{
            d->GetPDGEncoding(),
            d->GetAtomicNumber(),
            d->GetAtomicMass()
        };

        // 既要通过出生截断，也要达到核记录阈值，才需要谱系信息。
        const double infoMinEnergy =
            std::max(s.cfg.secondaryMinMeV, s.cfg.recoilRecordMinMeV) * MeV;

        for (auto* child : *children) {
            if (child->GetKineticEnergy() < infoMinEnergy) continue;

            const auto* cd = child->GetDefinition();
            if (cd->GetAtomicNumber()<1 || cd->GetAtomicMass()<2) continue;

            if (!child->GetUserInformation()) {
            child->SetUserInformation(new RecoilInfo(parent));
            }
        }
    }

        // 从这里开始处理当前轨迹本身的终点。
        const auto* info =
            dynamic_cast<const RecoilInfo*>(t->GetUserInformation());

        // 未写出生表的轨迹，也不写终点表。
        if (!info || !info->recorded) return;

        // 轨迹暂停不算真正结束；只记录终止状态。
        if (t->GetTrackStatus()!=fStopAndKill &&
            t->GetTrackStatus()!=fKillTrackAndSecondaries) return;

        const auto* step = t->GetStep();

        // 记录最后一步的限制过程，供诊断轨迹结束情况。
        // 它不一定单独、完整地解释轨迹为什么终止，还需结合 status。
        const auto* proc =
            step ? step->GetPostStepPoint()->GetProcessDefinedStep() : nullptr;

        auto pos = t->GetPosition();

        // GetTrackLength 是这条 Geant4 轨迹累计的实际路程。
        // 它不是可见损伤长度，也不是后续所有子核路程之和。
        // 若轨迹进入盐靶外部，外部路程在这里也未被排除。
        s.endings
            << s.eventID << '\t' << t->GetTrackID()
            << '\t' << t->GetKineticEnergy()/MeV
            << '\t' << t->GetTrackLength()/mm
            << '\t' << pos.x()/mm << '\t' << pos.y()/mm << '\t' << pos.z()/mm
            << '\t'
            << (info->killedByEnergyCut
                ? G4String("UserTrackingEnergyCut")
                : (proc ? proc->GetProcessName() : G4String("none")))
            << '\t' << static_cast<int>(t->GetTrackStatus()) << '\n';
    }
};


// ============================================================================
// 10. 主函数：读取参数、初始化、运行并关闭输出
// ============================================================================
int main(int argc, char** argv) {
    try {
        // --generate-only 只有放在命令最后才会被此解析方式识别。
        const bool generateOnly =
            argc>1 && std::string(argv[argc-1])=="--generate-only";

        // argc 包含程序名；去掉可选的末尾开关后，
        // 5 表示四个必需参数，7 表示还提供了半径和全长。
        const int nArgs = argc - (generateOnly ? 1 : 0);

        if (nArgs!=5 && nArgs!=7) {
            std::cerr
                << "Usage: GlashowRun energy_GeV events seed NEW_output_directory "
                   "[radius_m length_m] [--generate-only]\n";
            return 2;
        }

        // 聚合初始化前四个字段；其余字段使用 Config 中的默认值。
        Config c{
            std::stod(argv[1]),
            std::stoi(argv[2]),
            std::stol(argv[3]),
            fs::path(argv[4])
        };

        if (nArgs==7) {
            c.radiusM=std::stod(argv[5]);
            c.lengthM=std::stod(argv[6]);
        }

        c.generateOnly = generateOnly;

        // 环境变量优先于 Config 的默认出生截断值。
        // used 检查整个字符串都被解析，避免接受 "1MeV" 这类带单位输入。
        if (const char* value = std::getenv("GLASHOW_SECONDARY_MIN_MEV")) {
            std::size_t used = 0;
            const std::string text(value);
            c.secondaryMinMeV = std::stod(text, &used);

            if (used!=text.size() ||
                !std::isfinite(c.secondaryMinMeV) ||
                c.secondaryMinMeV<0.)
                throw std::runtime_error(
                    "GLASHOW_SECONDARY_MIN_MEV must be finite and nonnegative");
        }

        // GLASHOW_COMPRESS 只接受 "0" 和 "1"。
        if (const char* value = std::getenv("GLASHOW_COMPRESS")) {
            if (std::string(value)!="0" && std::string(value)!="1")
                throw std::runtime_error("GLASHOW_COMPRESS must be 0 or 1");

            c.compress = std::string(value)=="1";
        }

        // 本程序允许的入射中微子能区是 1–10 PeV，即 1e6–1e7 GeV。
        // 它与后面物理配置的 20 PeV 数值上限不是同一个限制。
        if (!std::isfinite(c.energyGeV) ||
            c.energyGeV<1e6 || c.energyGeV>1e7 ||
            c.events<1 ||
            c.seed<1 || c.seed>900000000 ||
            !std::isfinite(c.radiusM) ||
            !std::isfinite(c.lengthM) ||
            c.radiusM<=0. || c.lengthM<=0.)
            throw std::runtime_error(
                "Energy must be 1e6..1e7 GeV; positive events/geometry; "
                "seed 1..900000000");

        // 创建输出目录、打开文件、写初始元数据。
        State state(c);
        state.metadata(false);

        // 设置 Geant4 的随机种子；PYTHIA 种子在 Generator 中单独设置。
        G4Random::setTheSeed(c.seed);

        // 在构造强子物理表前设置全局强子能量上限。
        // 提高数值上限不是对 PeV 区模型精度的验证，
        // 也不意味着所有子模型都自动覆盖相同范围。
        G4HadronicParameters::Instance()->SetMaxEnergy(20.*PeV);

        // 这里显式使用 G4RunManager，按串行方式管理事件。
        // Geant4 安装版本标有 [MT]，不代表本程序自动启用了多线程。
        auto manager=std::make_unique<G4RunManager>();

        manager->SetUserInitialization(new Detector(c));

        // 使用参考物理表，再替换其电磁物理构造器。
        // 构造参数 1 是详细输出等级，不是线程数。
        auto* physics=new FTFP_BERT_HP(1);
        physics->ReplacePhysics(new G4EmStandardPhysics_option4(1));

        // 电磁物理模型/物理表的最高能量。
        // 与下面的次级粒子产生阈值是不同设置。
        G4EmParameters::Instance()->SetMaxEnergy(20.*PeV);

        // 保留原来的射程截断。
        // Geant4 将它转换为不同材料中的电磁次级粒子产生能量阈值。
        physics->SetDefaultCutValue(0.005*mm);

        // 提高“射程截断转换为能量阈值”时允许的最低能量。
        // 与现有的出生/追踪截断共用 secondaryMinMeV。
        // secondaryMinMeV = 1000 时，下限为 1 GeV。
        if (c.secondaryMinMeV > 0.) {
    const G4double productionFloor = c.secondaryMinMeV * MeV;

            // 本方案按 Geant4 11.3.2 的转换器范围设置。
        // 该版本这里的能量上界最多为 10 GeV，且必须严格大于下界。
            if (productionFloor >= 10.*GeV) {
                throw std::runtime_error(
                    "EM production floor must be below 10000 MeV");
            }

            G4ProductionCutsTable::GetProductionCutsTable()
                ->SetEnergyRange(productionFloor, 10.*GeV);

            std::cout
                << "EM production-cut lower bound = "
                << productionFloor / MeV << " MeV\n";
        }

        // 查找物理表中的中子跟踪截断构造器。
        for (int i=0; ; ++i) {
            const auto* part=physics->GetPhysics(i);
            if (!part) break;

            if (const auto* cut=
                    dynamic_cast<const G4NeutronTrackingCut*>(part)) {
                // 将该组件的中子时间限制设为 1 ms。
                const_cast<G4NeutronTrackingCut*>(cut)
                    ->SetTimeLimit(1.*ms);

                // 该组件的动能限制设为 0。
                // 这不关闭前面的 SecondaryCut 出生筛选。
                const_cast<G4NeutronTrackingCut*>(cut)
                    ->SetKineticEnergyLimit(0.);
            }
        }

        manager->SetUserInitialization(physics);

        // 注册用户回调，之后由 Geant4 在对应阶段自动调用。
        auto* generator = new Generator(state);
        manager->SetUserAction(generator);
        manager->SetUserAction(new Events(state));
        manager->SetUserAction(new Tracking(state));
        manager->SetUserAction(new TrackingEnergyCut(state));

        // 对所有次级粒子执行出生动能筛选。
        // 此份代码默认 10000 MeV；最终值以环境变量覆盖后的配置为准。
        manager->SetUserAction(new SecondaryCut(state));

        // 建立几何、粒子、物理过程及所需物理表。
        manager->Initialize();

        if (generateOnly) {
            // 只生成初级粒子，不调用 BeamOn，因此不进行材料输运。
            // 此模式不会执行正常输运的事件结束回调，
            // completed_events 可以为 0，而 generated_events 等于请求数。
            for (int i=0; i<c.events; ++i) {
                G4Event event(i);
                generator->GeneratePrimaries(&event);
            }

            state.closeOutputs();
            state.metadata(true);

            std::cout
                << "GENERATION ONLY COMPLETE; no material transport: "
                << c.output << '\n';
            return 0;
        }

        // 先完成运行初始化和物理表构建，但不生成任何事件。
        // 这样可以在真正开始耗时模拟之前，查看实际采用的产生阈值。
        manager->BeamOn(0);

        // 打印各材料对应的射程截断和能量截断。
        G4ProductionCutsTable::GetProductionCutsTable()->DumpCouples();

        // 正式模拟请求的事件。
        manager->BeamOn(c.events);

        // 防止未完成全部事件却写入成功标记。
        if (state.completed!=c.events)
            throw std::runtime_error("Incomplete run");

        // 先成功关闭数据文件，再声明整个任务完成。
        state.closeOutputs();
        state.metadata(true);

        std::cout << "COMPLETE: " << c.output << '\n';
        return 0;

    } catch (const std::exception& e) {
        // 捕获标准 C++ 异常并以非零状态退出。
        // 本程序没有额外安装 Ctrl+C 信号处理器；
        // 用户中断不保证经过这里，也不保证所有缓冲区正常关闭。
        std::cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }
}