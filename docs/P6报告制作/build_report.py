from pathlib import Path
import json,zipfile,hashlib,shutil,math
from docx import Document
from docx.shared import Pt,Cm,RGBColor
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.style import WD_STYLE_TYPE
from PIL import Image,ImageDraw,ImageFont

HERE=Path(__file__).resolve().parent
ROOT=HERE.parent.parent
REF=HERE/'课程要求参考副本.docx'
OUT=HERE.parent/'P6课程设计报告_李佳烨_面向对象修订版.docx'
sections=json.loads((HERE/'report-content.json').read_text(encoding='utf-8'))

# 使用简单UML符号绘制与源码对应的群策略类图。
im=Image.new('RGB',(1600,620),'white');dr=ImageDraw.Draw(im)
font=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',30)
small=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',26)
bold=ImageFont.truetype('C:/Windows/Fonts/msyhbd.ttc',31)
def textcenter(x,y,text,f=font):
 box=dr.textbbox((0,0),text,font=f);dr.text((x-(box[2]-box[0])/2,y),text,font=f,fill='#111111')
def box(x,y,w,h,title,detail):
 dr.rectangle((x,y,x+w,y+h),outline='#333333',width=3)
 dr.line((x,y+50,x+w,y+50),fill='#333333',width=2)
 textcenter(x+w/2,y+7,title,bold)
 for k,line in enumerate(detail):textcenter(x+w/2,y+62+k*35,line,small)
def line(points,dashed=False):
 for a,b in zip(points,points[1:]):
  if not dashed:dr.line((a,b),fill='#333333',width=3);continue
  length=math.dist(a,b)
  for t in range(0,int(length),17):
   u=min(t+10,length);dr.line((a[0]+(b[0]-a[0])*t/length,a[1]+(b[1]-a[1])*t/length,a[0]+(b[0]-a[0])*u/length,a[1]+(b[1]-a[1])*u/length),fill='#333333',width=3)
def arrow(x,y,dx,dy,hollow=False):
 v=(-dy,dx);tip=(x,y);a=(x-dx*18+v[0]*11,y-dy*18+v[1]*11);b=(x-dx*18-v[0]*11,y-dy*18-v[1]*11)
 if hollow:dr.polygon([tip,a,b],fill='white',outline='#333333',width=3)
 else:dr.line([a,tip,b],fill='#333333',width=3)
box(75,18,520,125,'GroupManagerLJY',['policy(style)  /  switchMode(...)'])
box(1010,18,515,125,'GroupLJY',['style  /  members  /  parentGroupId'])
line([(595,80),(1010,80)]);arrow(1010,80,1,0);textcenter(800,32,'查询与修改',small)
box(480,245,640,135,'GroupPolicyLJY',['抽象接口  canApply(...) / canKick(...)'])
line([(335,143),(335,310),(480,310)],True);arrow(480,310,1,0);textcenter(190,209,'依赖基类接口',small)
box(55,475,630,125,'QQGroupPolicyLJY',['申请  /  管理员  /  一级子群'])
box(915,475,630,125,'WeChatGroupPolicyLJY',['邀请  /  不新增子群'])
line([(370,475),(370,422),(800,422),(800,380)]);arrow(800,380,0,-1,True)
line([(1230,475),(1230,422),(800,422)])
im.save(HERE/'群策略类图.png',dpi=(240,240))
# 核心业务关系：实心菱形表示组成，普通连线为编号关联。
im=Image.new('RGB',(1600,700),'white');dr=ImageDraw.Draw(im)
box(30,30,500,140,'PersonLJY',['- personId / birthDate','+ personId() / location()'])
box(940,30,630,140,'ServiceAccountLJY',['- accountId / personId / serviceType','+ accountId() / personId()'])
line([(530,100),(940,100)]);textcenter(730,43,'关联：personId',small)
textcenter(560,109,'1',small);textcenter(895,109,'0..3',small)
box(30,445,630,155,'FriendRelationLJY',['- firstAccountId / secondAccountId','+ remarkFor(ownerAccountId)'])
line([(660,520),(800,520),(800,152),(940,152)])
textcenter(620,295,'每条关系关联2个账号',small)
box(940,240,630,140,'GroupMemberLJY',['- accountId / administrator','+ isAdministrator()'])
line([(1255,240),(1255,170)]);textcenter(1400,189,'关联：accountId',small)
box(940,480,630,140,'GroupLJY',['- members : vector<GroupMemberLJY>','+ findMember(accountId)'])
dr.polygon([(1255,480),(1267,462),(1255,444),(1243,462)],fill='#333333')
line([(1255,444),(1255,380)]);textcenter(1390,410,'组合：成员集合',small)
textcenter(1198,389,'0..*',small)
textcenter(800,653,'简化关系图：省略群主、父子群等关联；实体字段为私有，关系按内部编号建立。',small)
im.save(HERE/'核心业务对象关系.png',dpi=(240,240))

D=Document(REF)
body=D.element.body
children=list(body)
# 只保留原文中的封面与报告表格；删除模板说明。
for e in children[:88]+children[107:-1]:body.remove(e)
cover,table=D.tables
# 将旧模板的独立分页段落改为正文表头前分页，避免产生空白页。
for p in list(D.paragraphs):
 if p._p.xpath('.//w:br[@w:type="page"]'):body.remove(p._p)
for row,value in [(1,'2班'),(2,'55250229'),(3,'李佳烨')]:
 cell=cover.cell(row,1)
 p=cell.paragraphs[0]
 for run in p.runs:run.text=''
 run=p.add_run(value);run.font.name='Times New Roman';run.font.size=Pt(14);run.bold=True
 run._element.get_or_add_rPr().rFonts.set(qn('w:eastAsia'),'宋体')
for p in D.paragraphs:
 if '2025-2026' in p.text:
  for run in p.runs:run.text=run.text.replace('2025-2026','2026—2027')
  if '2026' not in p.text:p.add_run('2026—2027学年第1学期')

for name,size,fontname in [('报告一级标题',14,'黑体'),('报告小标题',12,'黑体')]:
 st=D.styles.add_style(name,WD_STYLE_TYPE.PARAGRAPH)
 st.base_style=D.styles['Normal'];st.font.name='Times New Roman';st.font.size=Pt(size);st.font.bold=True;st.font.color.rgb=RGBColor(0,0,0)
 st._element.get_or_add_rPr().rFonts.set(qn('w:eastAsia'),fontname)
 st.paragraph_format.keep_with_next=True
 st.paragraph_format.page_break_before=False
 st.paragraph_format.space_before=Pt(6);st.paragraph_format.space_after=Pt(4)
 st.paragraph_format.first_line_indent=Pt(0);st.paragraph_format.line_spacing=1.15
 st._element.get_or_add_pPr().append(OxmlElement('w:outlineLvl'))
 st._element.pPr[-1].set(qn('w:val'),'0' if size==14 else '1')

def runfont(r,size=12,bold=False):
 r.font.name='Times New Roman';r.font.size=Pt(size);r.font.bold=bold;r.font.color.rgb=RGBColor(0,0,0)
 r._element.get_or_add_rPr().rFonts.set(qn('w:eastAsia'),'宋体')
def paragraph(cell,text,style=None):
 p=cell.add_paragraph(style=style)
 f=p.paragraph_format
 f.left_indent=Pt(0);f.right_indent=Pt(0)
 f.keep_together=False;f.widow_control=True;f.page_break_before=False;f.keep_with_next=bool(style)
 if not style:
  f.first_line_indent=Pt(24);f.space_before=Pt(0);f.space_after=Pt(3);f.line_spacing=1.15
  p.alignment=WD_ALIGN_PARAGRAPH.JUSTIFY
  runfont(p.add_run(text))
 else:p.add_run(text)
 return p
for row,(title,blocks) in zip(list(table.rows)[1:],sections):
 trpr=row._tr.get_or_add_trPr()
 for tag in ['trHeight','cantSplit']:
  for el in list(trpr.findall(qn('w:'+tag))):trpr.remove(el)
 off=OxmlElement('w:cantSplit');off.set(qn('w:val'),'0');trpr.append(off)
 c=row.cells[0]
 cm=OxmlElement('w:tcMar')
 for side in ['top','bottom','left','right']:
  edge=OxmlElement('w:'+side);edge.set(qn('w:w'),'90');edge.set(qn('w:type'),'dxa');cm.append(edge)
 c._tc.get_or_add_tcPr().append(cm)
 for child in list(c._tc):
  if child.tag!=qn('w:tcPr'):c._tc.remove(child)
 paragraph(c,title,'报告一级标题')
 for kind,text in blocks:
  if kind=='p':paragraph(c,text)
  elif kind=='h':paragraph(c,text,'报告小标题')
  else:
   p=c.add_paragraph();p.alignment=WD_ALIGN_PARAGRAPH.CENTER;p.paragraph_format.keep_with_next=True;p.paragraph_format.space_after=Pt(2)
   name='群策略类图.png' if text=='class' else '核心业务对象关系.png'
   pic=p.add_run().add_picture(str(HERE/name),width=Cm(15.0))
   pic._inline.docPr.set('descr','群管理器依赖群策略抽象接口，QQ和微信策略继承该接口' if text=='class' else '人员与服务账号关联，好友关系关联两端账号，群组合群成员，群成员关联服务账号')
   p=c.add_paragraph('图2  群策略与运行时多态' if text=='class' else '图1  核心业务对象关系')
   p.alignment=WD_ALIGN_PARAGRAPH.CENTER;p.paragraph_format.space_after=Pt(5)
   for r in p.runs:runfont(r,10.5)
# 明确表头与下一行相连，避免旧模板孤立表头。
for c in table.rows[0].cells:
 for p in c.paragraphs:p.paragraph_format.keep_with_next=True;p.paragraph_format.page_break_before=True
# 保留原黑色表框，内容行允许跨页，增加微量内边距。
mar=table._tbl.tblPr.find(qn('w:tblCellMar'))
for side,value in [('top','70'),('bottom','70'),('left','110'),('right','110')]:
 e=mar.find(qn('w:'+side))
 if e is None:e=OxmlElement('w:'+side);mar.append(e)
 e.set(qn('w:w'),value);e.set(qn('w:type'),'dxa')
# 文本模板原有章节编号属于自动编号，正文使用确定的标题文字。
D.sections[0].different_first_page_header_footer=True
for header in [D.sections[0].header,D.sections[0].first_page_header]:
 hp=header.paragraphs[0];hp.style=D.styles['Normal']
 border=OxmlElement('w:pBdr')
 for side in ['top','bottom','left','right']:
  edge=OxmlElement('w:'+side);edge.set(qn('w:val'),'nil');border.append(edge)
 hp._p.get_or_add_pPr().append(border)
footer=D.sections[0].footer.paragraphs[0];footer.alignment=WD_ALIGN_PARAGRAPH.CENTER
r=footer.add_run();runfont(r,10.5)
fld=OxmlElement('w:fldSimple');fld.set(qn('w:instr'),'PAGE');r._r.addnext(fld)
D.core_properties.title='模拟即时通信系统实现'
D.core_properties.author='李佳烨'
D.core_properties.subject='面向对象程序课程设计实验报告'
D.core_properties.comments=''
update=OxmlElement('w:updateFields');update.set(qn('w:val'),'true');D.settings.element.append(update)
intermediate=HERE/'report-working.docx';D.save(intermediate)
# 仅替换计划内的部件，保留模板其余元数据与关系字节。
mutable={'word/document.xml','word/styles.xml','word/settings.xml','word/_rels/document.xml.rels','[Content_Types].xml','docProps/core.xml'}
with zipfile.ZipFile(REF) as original,zipfile.ZipFile(intermediate) as new,zipfile.ZipFile(OUT,'w',zipfile.ZIP_DEFLATED) as out:
 oldnames=set(original.namelist())
 for name in original.namelist():
  out.writestr(name,new.read(name) if name in mutable and name in new.namelist() else original.read(name))
 for name in new.namelist():
  if name not in oldnames:out.writestr(name,new.read(name))
print(OUT)
