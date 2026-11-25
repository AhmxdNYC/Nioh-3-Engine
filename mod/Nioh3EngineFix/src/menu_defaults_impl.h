// #83530 page-scoped restoration using defaults embedded from packaged INIs.
#pragma once
char g_defaultsNote[200] = "A: restore this page. Live values apply now; startup options apply after restart. Saved on menu close.";
int DefaultCatalogIndex(int id) {
    for(int i=0;i<kConfigN;++i) if(kConfigSettings[i].legacy==id) return i;
    return -1;
}
const char* DefaultConfigText(int index) {
    const auto& d=kConfigSettings[index];
    return shipped_defaults::Get(d.section,d.key,d.initial);
}
int DefaultPsWhat(int id) {
    if(id==kFrameMeterRow)return kFrameMeterWhat;
    if(id==kKiWindowRow)return kKiWindowWhat;
    if(id==kFrameMeterStyleRow)return kFrameMeterStyleWhat;
    if(id==kFrameMeterSizeRow)return kFrameMeterSizeWhat;
    if(id==kTimeOfDayRow)return kTimeOfDayWhat;
    if(id==kWeatherRow)return kWeatherWhat;
    if(id>=kPsRowFirst&&id<kPsRowFirst+kPsToggleN)return kPsToggles[id-kPsRowFirst].what;
    return -1;
}
const char* DefaultPsKey(int what) {
    switch(what) {
    case PS_FRAME_METER:return "FrameMeter";case PS_PLAYER_METER:return "PlayerFrameMeter";
    case PS_NEG_KI_METER:return "NegKiMeter";case PS_FRAME_LEGEND:return "FrameMeterLegend";
    case PS_FRAME_READOUT:return "FrameMeterReadout";case PS_FRAME_KI:return "FrameMeterKi";
    case PS_KI_WINDOW:return "KiWindow";case PS_FRAME_IFR:return "FrameMeterIFrames";
    case PS_FRAME_STYLE:return "FrameMeterStyle";case PS_FRAME_SIZE:return "FrameMeterRingSize";
    case PS_TIME_OF_DAY:return "TimeOfDayMin";case PS_WEATHER:return "Weather";
    default:break;
    }
    static const char* keys[]={"FrameMeterPlayerRow","FrameMeterKeyStartup","FrameMeterKeyActive","FrameMeterKeyRecovery","FrameMeterKeyUnblockable","FrameMeterKeyGrab","FrameMeterKeyOther","FrameMeterKeyTiming","FrameMeterKeyKi","FrameMeterKeyIFrames"};
    return what>=PS_FRAME_PROW&&what<=PS_KEY_IFR?keys[what-PS_FRAME_PROW]:"";
}
bool ResettableRow(int id) {
    if(id>=kSettingRow&&id<kSettingRow+kConfigN) return kConfigSettings[id-kSettingRow].kind!=setting_ui::Kind::Manual;
    if(id<0||id==kPage||id==kSave||id==kClose||ReadOnly(id))return false;
    return DefaultCatalogIndex(id)>=0 || DefaultPsWhat(id)>=0 || id==kRd || id==kVrs ||
        (id>=kCamRowFirst&&id<kCamRowFirst+kCamRowN);
}
void ResetLegacyDefault(int id,const char* text) {
    const int raw=atoi(text);auto& o=osd::Overlay();
    if(IsExp(id)) {
        const int e=id-kXW6;
        const bool on=raw<0?g_host.wine:raw==1;
        g_expIni[e]=raw<0?(on?1:0):raw;g_expIni0[e]=g_expIni[e];g_expAuto[e]=raw<0;
        if(g_host.exp[e].enable)*g_host.exp[e].enable=on?1u:0u;
    } else if(IsEp(id))extraperf::SetLevel(id-kEp0,raw);
    else switch(id) {
    case kVrr:case kHitStop:case kW3:case kW3Mode:case kL1:case kF1:case kWk:
        if(Available(id))SetLiveBool(id,raw==1);break;
    case kR1:if(Available(id))*g_host.r1Timeout=atoi(shipped_defaults::Get("Patches","R1","0"))==1?raw:0;break;
    case kFps:o.fpsCounter=raw;break;case kFrameTime:o.frameTime=raw;break;case kSpeed:o.speed=raw;break;
    case kPos:o.pos=!strcmp(text,"TopRight")?1:!strcmp(text,"BottomLeft")?2:!strcmp(text,"BottomRight")?3:0;break;
    case kScale:o.scale=(float)atof(text);break;
    case kOffX:o.offX=raw;o.offUser=true;break;case kOffY:o.offY=raw;o.offUser=true;break;
    case kHint:g_cfg.pauseHint=raw;break;case kFreeze:g_cfg.freezeGame=raw;break;
    case kAutoCont:if(Available(id))*g_host.autoContinue=(uint8_t)raw;break;
    case kWorkerCap:g_bootWorkerCap=raw;break;case kP12:g_bootP12=raw;break;
    case kL1Wine:g_bootL1Wine=raw;break;case kFgWine:g_bootFgWine=raw;break;
    case kLat:o.maxLatencyAuto=raw<0;o.maxLatency=raw<0?(g_host.wine?1:0):raw;g_latBoot=o.maxLatency;break;
    case kGpu:if(Available(id))gpuopt::SetBatching(raw==1);break;
    case kGR2a:case kGR2b:case kGR1:if(Available(id))gpuopt::SetRewrite(id-kGR2a,raw==1);break;
    case kEpPre:g_selectedEpPreset=raw;extraperf::ApplyPreset(raw);break;
    case kSharp:extraperf::SetSharpLevel(raw);break;
    default:break;
    }
}
void ResetConfigDefault(int index) {
    const auto& d=kConfigSettings[index];
    if(d.kind==setting_ui::Kind::Manual)return;
    const char* initial=DefaultConfigText(index);
    g_configValues.values[(size_t)index]={initial,true};
    if(d.legacy>=0)ResetLegacyDefault(d.legacy,initial);
    else ApplyConfigValue(index);
}
void ResetCurrentPage() {
    int rows[kMenuRowCap];const int n=PageRowsBody(rows,false);int changed=0,missing=0;
    bool camera=false;
    for(int i=0;i<n;++i) {
        const int id=rows[i];if(!ResettableRow(id))continue;
        if(id>=kSettingRow&&id<kSettingRow+kConfigN) {ResetConfigDefault(id-kSettingRow);++changed;continue;}
        const int index=DefaultCatalogIndex(id);
        if(index>=0){ResetConfigDefault(index);++changed;continue;}
        const int what=DefaultPsWhat(id);
        if(what>=0) {
            ResolveFrameMeter();char fallback[24];snprintf(fallback,sizeof fallback,"%d",PstToIni(what,PstDefault(what)));
            const int value=PstFromIni(what,atoi(shipped_defaults::Get("Training.Practice",DefaultPsKey(what),fallback)));
            if(g_pstSet&&g_pstSet(what,value,1)>=0)++changed;else ++missing;
        } else if(id==kRd){extraperf::SetRdPct((int)std::lround(atof(shipped_defaults::Get("ExtraPerf","RenderDistance","1.00"))*100));++changed;}
        else if(id==kVrs){g_vrsVals=vrsmenu::Values{};g_vrsPresetOn=false;VrsValuesChanged();++changed;}
        else if(id>=kCamRowFirst&&id<kCamRowFirst+kCamRowN)camera=true;
    }
    if(camera){ChangeCamera(kCamReset,0);++changed;}
    g_dirty=changed>0||g_dirty;
    snprintf(g_defaultsNote,sizeof g_defaultsNote,"Restored %d settings on this page%s. Saved on close; startup settings need a restart.",changed,missing?"; unavailable add-on settings were skipped":"");
    MLOG("defaults page %d: %s",g_pages.page,g_defaultsNote);
}
