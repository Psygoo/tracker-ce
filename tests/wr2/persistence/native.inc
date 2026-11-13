// CE control bodies, fake native copies/SD. Source is extracted, not reimplemented.
static MemoryModel hookModel;static MemoryIO hookIO;static Storage hookStorage{};
static Service service(hookIO,hookModel,hookStorage);
namespace wr2persist {bool beginSave();bool saveReady();bool finishSave();bool markInstrumentReplacement(unsigned);void resetWorkspace();void copyInstrument(unsigned,unsigned);void importInstrument(unsigned,const char*,const char*);void stageStartupOpen();void stageOpen();void finishOpen();void abort();}
constexpr int PATCH_SIZE=256,PROJECT_NAME_SIZE=64,PATTERN_INDEX_MAX=1,kSampleInstrumentsCount=48;
enum{fmIdle=0,fmNoOperation=1,fmSavingProjectToProjects,fmCopyWorkspaceToProjects,fmSaveEnd,fmSaveError,fmLoadEnd,fmImportingMod,fmImportModFile,fmCopyingInstrumentsEnd,fmImportSamplesError,fmCopyingInstrumentsInWorkspace,fmCopyInstrumentsInWorkspace};
static struct {struct{unsigned projectNotSavedFlag=0;}values;struct{bool isStreamed=false;}sampleMemory[48];struct{unsigned isActive=1;}instrument[48];}mtProject;
static unsigned nativeWrites=0,errors=0;static bool expectLoaded=false;static unsigned expectedWeight=0;
static struct {unsigned getBytesComplited(){return 0;}void clearBytesComplited(){}}fileTransfer;
static struct {void switchRamPatternsNow(){}}sequencer;
static void fixtureLog(const char*,...){}
#define LOG fixtureLog
#define LOGF fixtureLog
struct Meta {void copyPatternsMetadata(const char*,const char*){++nativeWrites;}void loadPatternsMetadata(){}};
struct FileManagerMock{
 char path[256]{},name[64]{};const char*workspaceName="A";const char*selectedFolder="Portable/Source/instruments";const char*selectedName="1 sample-0.wav.pti";bool folderAvailable=true,workspaceAvailable=true;int status=fmIdle,operation=fmNoOperation;Meta meta;
 bool isFilePtiInstrument(const char*p){return std::strstr(p,".pti")!=nullptr;}
 const char*getExplorerPath(){return selectedFolder;}const char*const*getExplorerList(){return &selectedName;}
 unsigned getSampleMemorySize(unsigned,unsigned){return 0;}void moveSampleMemory(unsigned,unsigned){}unsigned calcFirstSlotToMoveInMemory(unsigned){return 0;}
 void setInstrumentStructChanged(unsigned){}void saveInstrumentStructToWorkspace(unsigned){}bool saveSampleDataToWorkspace(unsigned){return true;}
 char*getCurrentProjectPath(){return path;}char*getCurrentProjectName(){return name;}
 int getStatus()const{return status;}int getCurrentOperation()const{return operation;}
 bool projectExist(const char*,const char*){return true;}
 void addCurrentProjectToRecentList(){}bool createCurrentProjectFolderTree(){return folderAvailable;}
 void setStatus(int n){status=n;}void setCurrentOperation(int n){operation=n;}
 void setProjectStructChanged(){}bool saveProjectToWorkspace(bool){return false;}
 Meta*getPatternsMetadata(){return &meta;}
 const char*getProjectNamefromProjectFile(){return workspaceName;}const char*getProjectPathfromProjectFile(){return "projects";}
 bool openProjectFromWorkspace(){if(!workspaceAvailable)return false;path[0]=name[0]=0;return true;}
 void startAudioEngine(){if(expectLoaded)CHECK(hookModel.rows[0].weight[0]==expectedWeight);else CHECK(hookModel.empty());}
 void loadProjectValues(){}bool needImportModFileAfterNewProject(){return false;}
 void clearImportModFileAfterNewProject(){}void resetCurrentOperationStep(){}
}newFileManager;
struct NativeFiles{
 bool exists(const char*){return true;}bool mkdir(int,const char*){++nativeWrites;return true;}
 bool removeOnlyDirFiles(const char*){++nativeWrites;return true;}bool removeDirWithFiles(const char*){++nativeWrites;return true;}
}nativeFiles;
static const char*cProjectsPatternsPathFormat="%s/%s/patterns",*cProjectsInstrumentsPathFormat="%s/%s/instruments",*cProjectsSamplesPathFormat="%s/%s/samples";
static const char*cProjectsPatternsMetadataFileFormat="%s/%s/patterns.dat",*cWorkspacePatternsMetadataFilePath="Workspace/patterns.dat";
class ProjectSave{
public:
 enum ProjectSaveStep{initStep,copyProjectFile,copyPatternFiles,copyInstrumentFiles,finishStep};ProjectSaveStep operationStep=initStep;
 int currentInstrument=0,currentPattern=0;uint8_t copyStep=0;uint32_t instrumentsSizeTotal=0,instrumentsSizeCopied=0;
 bool isIncrementalSave=false;NativeFiles*fileSystem=&nativeFiles;
 void update();void saveProjectInit();void saveProjectFinish();bool saveProject(char*,char*);void operationError();
 void clearExistingInstrumentList(){}void listProjectExistingInstruments(){}uint32_t calcWorkspaceInstrumentsSize(){return 0;}
 void copyProject(){++nativeWrites;}bool copyPaterns(int&){++nativeWrites;return true;}bool copyInstruments(int i,uint8_t&){++nativeWrites;hookIO.files[std::string(newFileManager.path)+"/"+newFileManager.name+"/instruments/"+std::to_string(i+1)+" "+hookModel.slots[i].name+".pti"]={1,2,static_cast<uint8_t>(i)};return true;}
 void stopOperationWithError(int error){++errors;newFileManager.setStatus(error);newFileManager.setCurrentOperation(fmNoOperation);service.abort();}
};
class WorkspaceProjectLoad{public:void loadProjectFromWorkspaceFinish();};
class Interface{public:void openStartupProject();void openNewProject(){wr2persist::abort();hookModel.clear();}};
#define model hookModel
#define io hookIO
#define scratch hookStorage
#include "native-wrappers.inc"
#undef model
#undef io
#undef scratch
static unsigned sampleChangedCalls=0,unblocks=0;static bool importingPti=true;static int expectAtUnblock=0;
namespace wr2native {static void sampleChanged(unsigned){++sampleChangedCalls;}}
static struct EngineMock {void clearBlockedInstrument(unsigned i){++unblocks;if(expectAtUnblock==1)CHECK(hookModel.rows[i].weight[0]==73);if(expectAtUnblock==2)CHECK(hookModel.rows[i].sliceCount==0);}}engine;
struct ImportData {const char*filePath;const char*fileName;bool ptiFile;};
static ImportData sampleImportDataBuff[48]{};
class InstrumentCopy {public:unsigned copySrcSlot=0,copyDestSlot=7,copySlotsCount=1,firstSlotToMoveInMemory=0;enum InstrumentCopyStep{initStep};int operationStep=initStep;void copyInstrumentFinish();bool copyInstrumentsInWorkspace(uint8_t,uint8_t,uint8_t);};
class SampleImport {
public:
 enum SampleImportStep {initStep,moveMemory,loadSampleData,saveInstrumentStructToWorkspace,saveInstrumentSampleToWorkspace,finishStep};
 SampleImportStep operationStep=initStep;unsigned currentInstrument=7,importStartSlot=7,importEndSlot=7;int importExplorerSlotFrom=0,importExplorerSlotTo=0,oldSamplesSize=0;unsigned memoryToImportDone=0;bool done=false,failed=false;unsigned initializations=0;
 void update();void importSamplesToWorkspaceInit(){++initializations;if(importingPti)hookModel.erase(currentInstrument);}
 void loadImportingSamplesData(ImportData*data,const char*folder,const char*const*names){data[currentInstrument]={folder,names[0],importingPti};}
 bool loadSampleDataToMemeory(unsigned,ImportData*){return true;}
 bool saveStreamedImportSampleToWorkspace(unsigned,ImportData*){return true;}
 void importSamplesToWorkspaceFinish(){done=true;}
 void stopOperationWithError(int){failed=true;newFileManager.setStatus(fmImportSamplesError);}
};

#include "native-hooks.inc"
static void nativeIntegrationTests(){
 hookIO=MemoryIO{};hookModel=MemoryModel{};hookModel.populate();service.abort();newFileManager=FileManagerMock{};
 std::strcpy(newFileManager.path,"projects");std::strcpy(newFileManager.name,"A");
 save(hookIO,hookModel,"projects/A");const auto source=hookIO.files["projects/A/wr2_groups.wr2"];
 setWeight(hookModel.rows[0],0,0,73);expectedWeight=hookModel.rows[0].weight[0];
 char destPath[]="projects",destName[]="B";ProjectSave saveOperation;
 CHECK(saveOperation.saveProject(destPath,destName));CHECK(std::string(newFileManager.name)=="B");nativeWrites=errors=0;
 saveOperation.update();CHECK(service.saveReady()&&hookIO.files.count("projects/B/wr2_groups.wr2.pending"));
 for(unsigned tick=0;tick<60&&newFileManager.status!=fmSaveEnd;++tick)saveOperation.update();
 CHECK(newFileManager.status==fmSaveEnd&&errors==0);CHECK(hookIO.files["projects/A/wr2_groups.wr2"]==source);
 CHECK(hookIO.files.count("projects/B/wr2_groups.wr2"));
 // Real wrapper captures B before native Workspace clears/restores stale serialized A.
 wr2persist::stageOpen();newFileManager.openProjectFromWorkspace();WorkspaceProjectLoad loadOperation;expectLoaded=true;loadOperation.loadProjectFromWorkspaceFinish();
 CHECK(std::string(newFileManager.name)=="A");CHECK(service.result()==wr2::persist::Result::Loaded&&hookModel.rows[0].weight[0]==73);
 // Aborted loads must not publish into a later Workspace/New context.
 wr2persist::stageOpen();wr2persist::abort();hookModel.clear();expectLoaded=false;loadOperation.loadProjectFromWorkspaceFinish();CHECK(hookModel.empty());
 // A completed native regular Save is also recovered on the next boot.
 newFileManager.status=fmIdle;newFileManager.operation=fmNoOperation;std::strcpy(newFileManager.path,"projects");std::strcpy(newFileManager.name,"B");
 hookModel.populate();setWeight(hookModel.rows[0],0,0,73);ProjectSave regular;CHECK(regular.saveProject(nullptr,nullptr));
 for(unsigned tick=0;tick<60&&newFileManager.status!=fmSaveEnd;++tick)regular.update();CHECK(newFileManager.status==fmSaveEnd&&!hookIO.files.count("projects/B/wr2_groups.wr2.pending"));
 // Boot uses the materialized Workspace path, without an early sidecar read.
 Interface ui;hookModel.clear();newFileManager.workspaceName="B";expectLoaded=true;expectedWeight=73;
 ui.openStartupProject();CHECK(service.startupOpening()&&hookModel.empty());loadOperation.loadProjectFromWorkspaceFinish();
 CHECK(service.result()==wr2::persist::Result::Loaded&&hookModel.rows[0].weight[0]==73);
 // Boot without WR2, incompatible Workspace instrument/slices, pending save.
 expectLoaded=false;newFileManager.workspaceName="legacy";ui.openStartupProject();loadOperation.loadProjectFromWorkspaceFinish();CHECK(service.result()==wr2::persist::Result::Missing&&hookModel.empty());
 newFileManager.workspaceName="B";++hookModel.slots[47].frames;ui.openStartupProject();loadOperation.loadProjectFromWorkspaceFinish();CHECK(service.result()==wr2::persist::Result::Invalid&&hookModel.empty());--hookModel.slots[47].frames;
 ++hookModel.slots[0].cuts[1];ui.openStartupProject();loadOperation.loadProjectFromWorkspaceFinish();CHECK(service.result()==wr2::persist::Result::Invalid&&hookModel.empty());--hookModel.slots[0].cuts[1];
 hookIO.files["projects/B/wr2_groups.wr2.pending"]={0};ui.openStartupProject();loadOperation.loadProjectFromWorkspaceFinish();CHECK(service.result()==wr2::persist::Result::Interrupted&&hookModel.empty());hookIO.files.erase("projects/B/wr2_groups.wr2.pending");
 // Failed startup falls back to New; no later unsolicited sidecar restoration.
 newFileManager.workspaceAvailable=false;ui.openStartupProject();CHECK(!service.startupOpening());loadOperation.loadProjectFromWorkspaceFinish();CHECK(hookModel.empty());newFileManager.workspaceAvailable=true;
 // Explicit Open still owns B even while serialized Workspace points at A.
 expectedWeight=73;expectLoaded=true;std::strcpy(newFileManager.path,"projects");std::strcpy(newFileManager.name,"B");newFileManager.workspaceName="A";
 wr2persist::stageOpen();newFileManager.openProjectFromWorkspace();loadOperation.loadProjectFromWorkspaceFinish();CHECK(hookModel.rows[0].weight[0]==73);
 expectLoaded=false;
 std::puts("PASS startup: actual startup entry/finish, restart after Save As, missing sidecar, incompatible last instrument/cuts, pending guard, failed startup/New, explicit Open source precedence");
 // Upstream mkdir failure must stop before any native copy or WR2 sidecar transaction.
 newFileManager.status=fmIdle;newFileManager.operation=fmNoOperation;newFileManager.folderAvailable=false;
 ProjectSave mkdirFailure;CHECK(mkdirFailure.saveProject(destPath,destName));nativeWrites=errors=0;
 mkdirFailure.update();CHECK(errors==1&&nativeWrites==0&&!service.saveReady());newFileManager.folderAvailable=true;
 // Guard failure before native copying is an error, not a successful Save.
 newFileManager.status=fmIdle;newFileManager.operation=fmNoOperation;ProjectSave failing;CHECK(failing.saveProject(destPath,destName));
 hookIO.reset();hookIO.fault=1;nativeWrites=errors=0;failing.update();CHECK(errors==1&&nativeWrites==0&&!service.saveReady());
 // Full native SampleImport::update drives RAM/stream materialization stubs,
 // then actual import wrapper and portable root/companion validator before unblock.
 hookIO.reset();hookModel.populate();setWeight(hookModel.rows[0],0,0,73);
 ptiFiles(hookIO,hookModel,"Portable/Source");savePortable(hookIO,hookModel,"Portable/Source");
 hookModel.slots[7]=hookModel.slots[0];importingPti=true;expectAtUnblock=1;
 for(bool streamed:{false,true}){
  mtProject.sampleMemory[7].isStreamed=streamed;SampleImport op;
  for(unsigned n=0;n<10&&!op.done;++n)op.update();CHECK(op.done&&hookModel.rows[7].weight[0]==73);
 }
 // Same native state machine imports a renamed two-file bundle without a root.
 hookIO.files["CopiedCard/Bundle/Renamed.pti"]=hookIO.files["Portable/Source/instruments/1 sample-0.wav.pti"];
 hookIO.files["CopiedCard/Bundle/Renamed.pti.w2i"]=hookIO.files["Portable/Source/instruments/1 sample-0.wav.pti.w2i"];
 newFileManager.selectedFolder="CopiedCard/Bundle";newFileManager.selectedName="Renamed.pti";SampleImport independentImport;
 for(unsigned n=0;n<10&&!independentImport.done;++n)independentImport.update();CHECK(independentImport.done&&hookModel.rows[7].weight[0]==73);
 newFileManager.selectedFolder="Portable/Source/instruments";newFileManager.selectedName="1 sample-0.wav.pti";
 // Valid native PTI without portable metadata replaces the prior groups.
 hookIO.files.erase("Portable/Source/instruments/1 sample-0.wav.pti.w2i");hookIO.files.erase("Portable/Source/wr2_companions/instrument_01.a.w2i");hookIO.files.erase("Portable/Source/wr2_companions/instrument_01.b.w2i");expectAtUnblock=2;SampleImport missing;
 for(unsigned n=0;n<10&&!missing.done;++n)missing.update();CHECK(missing.done&&hookModel.rows[7].sliceCount==0);
 // WAV preserves groups; its only WR2 hook invalidates temporary geometry.
 importingPti=false;newFileManager.selectedName="sample.wav";expectAtUnblock=0;hookModel.rows[7]=hookModel.rows[0];const auto wavGroups=hookModel.rows[7];const auto changedBefore=sampleChangedCalls;
 SampleImport wav;for(unsigned n=0;n<10&&!wav.done;++n)wav.update();CHECK(wav.done&&sampleChangedCalls==changedBefore+1&&!std::memcmp(&wavGroups,&hookModel.rows[7],sizeof(wavGroups)));
 InstrumentCopy copy;newFileManager.status=fmIdle;newFileManager.operation=fmNoOperation;CHECK(copy.copyInstrumentsInWorkspace(0,1,7));CHECK(readReplacements(hookIO)&(uint64_t(1)<<7));copy.copyInstrumentFinish();CHECK(!std::memcmp(&hookModel.rows[0],&hookModel.rows[7],sizeof(InstrumentState)));
 setWeight(hookModel.rows[7],0,0,66);CHECK(hookModel.rows[0].weight[0]==73);
 hookModel.rows[0].reset(48);copy.copyInstrumentFinish();CHECK(hookModel.rows[7].sliceCount==0);
 CHECK(unblocks==5);expectAtUnblock=0;
 // Replacement guard failure is detected before native init mutates memory/slots.
 importingPti=true;newFileManager.selectedName="1 sample-0.wav.pti";hookIO.reset();hookIO.fault=3;const auto oldSlot=hookModel.rows[7];SampleImport refused;refused.update();CHECK(refused.failed&&refused.initializations==0&&!std::memcmp(&oldSlot,&hookModel.rows[7],sizeof(oldSlot)));hookIO.reset();
 // Dirty Workspace PTI replacement with exactly the old identity is suppressed on boot.
 CHECK(clearReplacements(hookIO));hookIO.files.erase("projects/B/wr2_groups.wr2.pending");
 CHECK(wr2persist::markInstrumentReplacement(7));newFileManager.workspaceName="B";hookModel.clear();expectedWeight=73;expectLoaded=true;
 ui.openStartupProject();loadOperation.loadProjectFromWorkspaceFinish();CHECK(hookModel.rows[7].sliceCount==0);
 // A successful Save commits imported/copied metadata and removes the mask.
 newFileManager.status=fmIdle;newFileManager.operation=fmNoOperation;ProjectSave finalSave;CHECK(finalSave.saveProject(nullptr,nullptr));
 for(unsigned n=0;n<60&&newFileManager.status!=fmSaveEnd;++n)finalSave.update();CHECK(newFileManager.status==fmSaveEnd&&readReplacements(hookIO)==0);
 hookModel.clear();ui.openStartupProject();loadOperation.loadProjectFromWorkspaceFinish();CHECK(service.result()==wr2::persist::Result::Loaded);
 expectLoaded=false;
 std::puts("PASS native instrument lifecycle: full SampleImport update RAM/streamed, companion before unblock, PTI absent clears, WAV retains, actual Copy finish independent/empty source");

 std::puts("PASS native lifecycle fixture: actual Save As target, guard before native writes, restore after materialization/before audio, actual Open path beats stale Workspace path, abort isolation");
}
