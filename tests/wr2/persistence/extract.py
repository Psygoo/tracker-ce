from pathlib import Path
import sys,json,hashlib
r=Path(sys.argv[1]);o=Path(sys.argv[2]);o.mkdir(parents=True,exist_ok=True);manifest=[]
def body(path,sig):
 s=(r/path).read_text(encoding='utf-8');a=s.index(sig);b=s.index('{',a);e=b+1;d=1
 while d:d+=(s[e]=='{')-(s[e]=='}');e+=1
 text=s[a:e];manifest.append(dict(path=path,signature=sig,body_sha256=hashlib.sha256(text.encode()).hexdigest()));return text
chunks=[]
for sig in ['void ProjectSave::update()','void ProjectSave::saveProjectInit()','void ProjectSave::saveProjectFinish()','bool ProjectSave::saveProject(','void ProjectSave::operationError()']:
 chunks.append(body('core/FileManager/ProjectSave.cpp',sig))
chunks.append(body('core/FileManager/WorkspaceProjectLoad.cpp','void WorkspaceProjectLoad::loadProjectFromWorkspaceFinish()'))
chunks.append(body('core/interface/core/interfaceMisc.cpp','void Interface::openStartupProject()'))
chunks.append(body('core/FileManager/InstrumentCopy.cpp','void InstrumentCopy::copyInstrumentFinish()'))
chunks.append(body('core/FileManager/InstrumentCopy.cpp','bool InstrumentCopy::copyInstrumentsInWorkspace('))
chunks.append(body('core/FileManager/SampleImport.cpp','void SampleImport::update()'))
(o/'native-hooks.inc').write_text('\n'.join(chunks),encoding='utf-8')
chunks=[]
for sig in ['bool currentFolder(char*','bool beginSave()','bool saveReady()','bool finishSave()','void stageStartupOpen()','void stageOpen()','void finishOpen()','bool markInstrumentReplacement(','void resetWorkspace()','void copyInstrument(','void importInstrument(','void abort()']:
 chunks.append(body('core/FileManager/NativePersistence.cpp',sig))
(o/'native-wrappers.inc').write_text(chunks[0]+'\nnamespace wr2persist {\n'+'\n'.join(chunks[1:])+'\n}',encoding='utf-8')
(o/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
print('Extracted native Save/Save As/Workspace finish and persistence wrapper bodies')
