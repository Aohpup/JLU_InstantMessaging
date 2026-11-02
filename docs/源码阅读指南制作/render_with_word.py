from pathlib import Path
import importlib.util,sys,shutil
base=Path(__file__).resolve().parent
skill=Path('C:/Users/27918/.codex/plugins/cache/openai-primary-runtime/documents/26.1025.11240/skills/documents/render_docx.py')
spec=importlib.util.spec_from_file_location('renderer',skill)
renderer=importlib.util.module_from_spec(spec);spec.loader.exec_module(renderer)
source=Path(sys.argv[1]).resolve();pdf=Path(sys.argv[2]).resolve();out=Path(sys.argv[3]).resolve()
def word_pdf(doc_path,user_profile,convert_tmp_dir,stem,verbose):
 target=Path(convert_tmp_dir)/(stem+'.pdf');shutil.copy2(pdf,target)
 return str(target),'Microsoft Word native PDF supplied; LibreOffice unavailable on PATH.'
renderer.convert_to_pdf=word_pdf
images=renderer.rasterize(str(source),str(out),120,False,True)
print('Rendered pages:',len(images))
