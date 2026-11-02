from pathlib import Path
import re, math, json
from PIL import Image,ImageDraw,ImageFont
from docx import Document
from docx.shared import Inches,Pt,RGBColor
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT,WD_CELL_VERTICAL_ALIGNMENT
base=Path(__file__).resolve().parent; root=base.parent.parent
md=root/'docs/源码阅读指南_面向对象_SQLite_Qt.md'
fontpath='C:/Windows/Fonts/msyh.ttc'
def diagram(name,kind):
 w,h=(1580,500) if kind==1 else ((1580,560) if kind==2 else (1580,640))
 im=Image.new('RGB',(w,h),'white'); d=ImageDraw.Draw(im)
 f=ImageFont.truetype(fontpath,34); fs=ImageFont.truetype(fontpath,29)
 def box(rect,lines):
  d.rounded_rectangle(rect,16,fill='#f1f7f4',outline='#32715c',width=3)
  cx=(rect[0]+rect[2])/2; total=len(lines)*46
  for i,t in enumerate(lines):
   b=d.textbbox((0,0),t,font=f);d.text((cx-(b[2]-b[0])/2,(rect[1]+rect[3]-total)/2+i*46),t,font=f,fill='#182b23')
 def arrow(points,empty=False,dashed=False):
  if dashed:
   for a,b in zip(points,points[1:]):
    length=math.dist(a,b)
    for j in range(0,int(length),22):
     t=j/length;u=min((j+12)/length,1)
     d.line((a[0]+(b[0]-a[0])*t,a[1]+(b[1]-a[1])*t,a[0]+(b[0]-a[0])*u,a[1]+(b[1]-a[1])*u),fill='#53635d',width=3)
  else:d.line(points,fill='#53635d',width=3)
  a,b=points[-2:];ang=math.atan2(b[1]-a[1],b[0]-a[0]);sz=19
  pts=[b,(b[0]-sz*math.cos(ang-.5),b[1]-sz*math.sin(ang-.5)),(b[0]-sz*math.cos(ang+.5),b[1]-sz*math.sin(ang+.5))]
  d.polygon(pts,fill='white' if empty else '#53635d',outline='#53635d',width=3)
 if kind==1:
  box((480,30,1100,150),['SocialServiceLJY','抽象基类'])
  for x,t in [(30,'QQServiceLJY'),(565,'WeChatServiceLJY'),(1100,'WeiboServiceLJY')]:
   box((x,310,x+450,420),[t]);arrow([(x+225,310),(x+225,230),(790,230),(790,150)],True)
  d.text((520,450),'空心三角箭头指向基类',font=fs,fill='#47564f')
 elif kind==2:
  box((485,20,1095,130),['GroupPolicyLJY','抽象基类'])
  box((90,280,690,390),['QQGroupPolicyLJY']);box((890,280,1490,390),['WeChatGroupPolicyLJY'])
  arrow([(390,280),(390,205),(790,205),(790,130)],True)
  arrow([(1190,280),(1190,205),(790,205),(790,130)],True)
  d.text((105,445),'GroupManager 拥有两个策略对象',font=f,fill='#182b23')
  d.text((105,495),'policy(style) 返回基类引用，再调用实际派生类的虚函数',font=fs,fill='#47564f')
 else:
  box((480,15,1100,115),['SessionManagerLJY'])
  for x,t in [(25,'AccountServiceLJY'),(565,'FriendManagerLJY'),(1105,'GroupManagerLJY')]:
   box((x,240,x+450,330),[t]);arrow([(790,115),(790,180),(x+225,180),(x+225,240)])
   arrow([(x+225,330),(x+225,390),(790,390),(790,450)],dashed=True)
  box((180,450,1400,625),['PlatformLJY','拥有人员、账号、好友关系、请求、服务与群','Group 内拥有 GroupMember 列表'])
  d.text((45,135),'拥有成员对象',font=fs,fill='#47564f');d.text((40,400),'引用同一个平台',font=fs,fill='#47564f')
 im.save(base/name)
for n,k in [('服务继承.png',1),('群策略.png',2),('对象协作.png',3)]:diagram(n,k)

def font(run,size=11,bold=False,name='宋体'):
 run.font.name='Times New Roman' if name=='宋体' else name
 run._element.get_or_add_rPr().rFonts.set(qn('w:eastAsia'),name)
 run.font.size=Pt(size);run.bold=bold;run.font.color.rgb=RGBColor(0,0,0)

doc=Document();sec=doc.sections[0];sec.page_width=Inches(8.5);sec.page_height=Inches(11)
sec.top_margin=Inches(.65);sec.bottom_margin=Inches(.65);sec.left_margin=Inches(.7);sec.right_margin=Inches(.7)
sec.header_distance=Inches(.25);sec.footer_distance=Inches(.25)
normal=doc.styles['Normal'];normal.font.name='Times New Roman';normal._element.rPr.rFonts.set(qn('w:eastAsia'),'宋体');normal.font.size=Pt(11)
normal.paragraph_format.line_spacing=1.19;normal.paragraph_format.space_after=Pt(7)
for n,size in [('Title',24),('Heading 1',17),('Heading 2',13)]:
 st=doc.styles[n];st.font.name='Microsoft YaHei';st._element.get_or_add_rPr().rFonts.set(qn('w:eastAsia'),'微软雅黑');st.font.size=Pt(size);st.font.color.rgb=RGBColor(0,0,0);st.font.bold=True
 st.paragraph_format.space_before=Pt(5);st.paragraph_format.space_after=Pt(10)
head=sec.header.paragraphs[0];font(head.add_run('JLU 即时通信平台  |  源码阅读指南'),9);head.paragraph_format.space_after=Pt(0)
foot=sec.footer.paragraphs[0];foot.alignment=WD_ALIGN_PARAGRAPH.CENTER;font(foot.add_run('第 '),9)
fld=OxmlElement('w:fldSimple');fld.set(qn('w:instr'),'PAGE');foot._p.append(fld);font(foot.add_run(' 页'),9)

def inline(p,text,size=11):
 parts=re.split(r'(\[[^\]]+\]\([^)]+\)|`[^`]+`|\*\*[^*]+\*\*)',text)
 for t in parts:
  if not t:continue
  m=re.fullmatch(r'\[([^\]]+)\]\(([^)]+)\)',t)
  if m:
   dest=(md.parent/m.group(2)).resolve().as_uri()
   h=OxmlElement('w:hyperlink');h.set(qn('r:id'),p.part.relate_to(dest,'http://schemas.openxmlformats.org/officeDocument/2006/relationships/hyperlink',is_external=True))
   r=OxmlElement('w:r');pr=OxmlElement('w:rPr');fo=OxmlElement('w:rFonts');fo.set(qn('w:ascii'),'Times New Roman');fo.set(qn('w:eastAsia'),'宋体');pr.append(fo)
   sz=OxmlElement('w:sz');sz.set(qn('w:val'),str(int(size*2)));pr.append(sz);color=OxmlElement('w:color');color.set(qn('w:val'),'205F49');pr.append(color);r.append(pr)
   tx=OxmlElement('w:t');tx.text=m.group(1);r.append(tx);h.append(r);p._p.append(h)
  else:
   bold=t.startswith('**');code=t.startswith('`');t=t[2:-2] if bold else (t[1:-1] if code else t)
   font(p.add_run(t),size,bold,'Consolas' if code and t.isascii() else '宋体')
def shade(el,color):
 sh=OxmlElement('w:shd');sh.set(qn('w:fill'),color);el.append(sh)
lines=md.read_text(encoding='utf-8-sig').splitlines();i=0;pending_break=False
while i<len(lines):
 line=lines[i];i+=1
 if not line.strip():continue
 if line=='<!-- PAGE -->':pending_break=True;continue
 if line.startswith('# '):doc.add_paragraph(line[2:],'Title');continue
 if line.startswith('## '):
  p=doc.add_paragraph(line[3:],'Heading 1');p.paragraph_format.page_break_before=pending_break;pending_break=False;continue
 if line.startswith('### '):doc.add_paragraph(line[4:],'Heading 2');continue
 if line.startswith('!['):
  m=re.fullmatch(r'!\[([^]]*)\]\(([^)]+)\)',line);p=doc.add_paragraph();p.alignment=WD_ALIGN_PARAGRAPH.CENTER
  p.add_run().add_picture(str(md.parent/m.group(2)),width=Inches(6.4 if '对象协作' in m.group(2) else 7.05));p.paragraph_format.space_after=Pt(4)
  p=doc.add_paragraph();p.alignment=WD_ALIGN_PARAGRAPH.CENTER;font(p.add_run(m.group(1)),9);continue
 if line.startswith('```'):
  code=[]
  while i<len(lines) and not lines[i].startswith('```'):code.append(lines[i]);i+=1
  i+=1
  for c in code:
   p=doc.add_paragraph();p.paragraph_format.space_after=Pt(0);p.paragraph_format.line_spacing=1.1;p.paragraph_format.keep_with_next=True
   p.paragraph_format.left_indent=Inches(.12);shade(p._p.get_or_add_pPr(),'F3F5F4')
   r=p.add_run(c);font(r,9.5,False,'Consolas');r._element.get_or_add_rPr().rFonts.set(qn('w:eastAsia'),'宋体')
  p=doc.add_paragraph();p.paragraph_format.space_after=Pt(3);p.paragraph_format.line_spacing=Pt(1);font(p.add_run(''),1);continue
 if line.startswith('|'):
  rows=[line]
  while i<len(lines) and lines[i].startswith('|'):rows.append(lines[i]);i+=1
  rows=[r for r in rows if not re.fullmatch(r'[| :\-]+',r)]
  data=[[c.strip() for c in r.strip('|').split('|')] for r in rows];n=len(data[0]);t=doc.add_table(rows=0,cols=n);t.alignment=WD_TABLE_ALIGNMENT.CENTER;t.autofit=False
  widths=([2.1,4.95] if n==2 else [2.05,2.5,2.5])
  for col,w in zip(t.columns,widths):col.width=Inches(w)
  borders=OxmlElement('w:tblBorders')
  for tag in ['top','left','bottom','right','insideH','insideV']:
   e=OxmlElement('w:'+tag);e.set(qn('w:val'),'single');e.set(qn('w:sz'),'4');e.set(qn('w:color'),'D1D8D4');borders.append(e)
  t._tbl.tblPr.append(borders)
  for j,row in enumerate(data):
   rr=t.add_row();pr=rr._tr.get_or_add_trPr();ns=OxmlElement('w:cantSplit');pr.append(ns)
   if j==0:pr.append(OxmlElement('w:tblHeader'))
   for k,val in enumerate(row):
    cell=rr.cells[k];cell.width=Inches(widths[k]);cell.vertical_alignment=WD_CELL_VERTICAL_ALIGNMENT.CENTER
    p=cell.paragraphs[0];p.paragraph_format.space_after=Pt(4);p.paragraph_format.space_before=Pt(4);p.paragraph_format.line_spacing=1.12
    inline(p,val,10)
    if j==0:
     shade(cell._tc.get_or_add_tcPr(),'EFF3F1')
     for r in p.runs:r.bold=True
  p=doc.add_paragraph();p.paragraph_format.space_after=Pt(3);p.paragraph_format.line_spacing=Pt(1);font(p.add_run(''),1);continue
 p=doc.add_paragraph();inline(p,line)
# Prevent lone heading and table captions; allow body paragraphs to flow naturally.
props=doc.core_properties;props.title='源码阅读指南：面向对象、SQLite 与 Qt';props.subject='基于当前源码的学习路径';props.author='李佳烨';props.keywords='C++,面向对象,继承,多态,SQLite,Qt,代码阅读'
output=md.with_suffix('.docx');doc.save(output)
# Validate links against project paths and document contents.
links=re.findall(r'\[[^\]]*\]\(([^)]+)\)',md.read_text(encoding='utf-8'))
missing=[x for x in links if not (md.parent/x).exists()]
qa={'markdown':str(md),'docx':str(output),'sections':len(re.findall(r'^## ',md.read_text(encoding='utf-8'),re.M)),'images':len(doc.inline_shapes),'tables':len(doc.tables),'links':len(links),'missing_links':missing,'note':'待 Word PDF 渲染后逐页检查'}
(base/'生成检查.json').write_text(json.dumps(qa,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(qa,ensure_ascii=False))
