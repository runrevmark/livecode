#include "prefix.h"

#include "globdefs.h"
#include "filedefs.h"
#include "objdefs.h"
#include "parsedef.h"

#include "execpt.h"
#include "util.h"
#include "mcerror.h"
#include "sellst.h"
#include "stack.h"
#include "card.h"
#include "image.h"
#include "widget.h"
#include "param.h"
#include "osspec.h"
#include "cmds.h"
#include "scriptpt.h"
#include "hndlrlst.h"
#include "debug.h"
#include "redraw.h"

#include "globals.h"
#include "context.h"

////////////////////////////////////////////////////////////////////////////////

static MCContext *MCwidgetcontext;
static MCWidget *MCwidgetobject;
static uint32_t MCwidgetcontextopacity;

////////////////////////////////////////////////////////////////////////////////

MCWidget::MCWidget(void)
{
	m_imp_script = nil;
	m_imp_handlers = nil;
}

MCWidget::MCWidget(const MCWidget& p_other)
	: MCControl(p_other)
{
	m_imp_script = nil;
	m_imp_handlers = nil;
	
	SetImplementation(p_other . m_imp_script);
}

MCWidget::~MCWidget(void)
{
	delete m_imp_script;
	delete m_imp_handlers;
}

Chunk_term MCWidget::gettype(void) const
{
	return CT_WIDGET;
}

const char *MCWidget::gettypestring(void)
{
	return MCwidgetstring;
}

void MCWidget::open(void)
{
	MCControl::open();
}

void MCWidget::close(void)
{
	MCControl::close();
}

void MCWidget::kfocus(void)
{
	MCControl::kfocus();
	if (getstate(CS_KFOCUSED))
		OnFocus();
}

void MCWidget::kunfocus(void)
{
	if (getstate(CS_KFOCUSED))
		OnUnfocus();
	MCControl::kunfocus();
}

Boolean MCWidget::kdown(const char *p_key_string, KeySym p_key)
{
	uint32_t t_modifiers;
	t_modifiers = 0;
	/*if ((MCmodifierstate & MS_CONTROL) != 0)
		t_modifiers |= kMCWidgetModifierKeyControl;
	if ((MCmodifierstate & MS_MOD1) != 0)
		t_modifiers |= kMCWidgetModifierKeyOption;
	if ((MCmodifierstate & MS_SHIFT) != 0)
		t_modifiers |= kMCWidgetModifierKeyShift;*/

	uint1 t_ascii_key;
	t_ascii_key = p_key_string[0];

	uint32_t t_sent_key;

	if (p_key > 0xff)
		t_sent_key = p_key;
	else if (t_ascii_key < ' ' || MCmodifierstate & MS_CONTROL)
		t_sent_key = (char)p_key;
	else
		t_sent_key = t_ascii_key;

	if (OnKeyPress(t_sent_key, t_modifiers))
		return True;

	return MCObject::kdown(p_key_string, p_key);
}

Boolean MCWidget::kup(const char *p_key_string, KeySym p_key)
{
	return False;
}

Boolean MCWidget::mdown(uint2 p_which)
{
	if (getstate(CS_MFOCUSED))
		return False;

	setstate(True, CS_MFOCUSED);

	switch(getstack() -> gettool(this))
	{
	case T_BROWSE:
		OnMouseDown(p_which, mx, my, 0);
		break;

	case T_POINTER:
		if (p_which == Button1)
			start(True);
		break;

	default:
		return False;
	}

	return True;
}

Boolean MCWidget::mup(uint2 p_which)
{
	if (!getstate(CS_MFOCUSED))
		return False;

	setstate(False, CS_MFOCUSED);

	switch(getstack() -> gettool(this))
	{
	case T_BROWSE:
		OnMouseUp(p_which, mx, my, 0);
		break;

	case T_POINTER:
		if (p_which == Button1)
			end();
		break;

	default:
		return False;
	}

	return True;
}

Boolean MCWidget::mfocus(int2 p_x, int2 p_y)
{
	if (!(getflag(F_VISIBLE) || MCshowinvisibles) ||
		(getflag(F_DISABLED) && (getstack() -> gettool(this) == T_BROWSE)))
		return False;

	return MCControl::mfocus(p_x, p_y);
}

void MCWidget::munfocus(void)
{
	MCControl::munfocus();
}

Boolean MCWidget::doubledown(uint2 p_which)
{
	return False;
}

Boolean MCWidget::doubleup(uint2 p_which)
{
	return False;
}

void MCWidget::timer(MCNameRef p_message, MCParameter *p_parameters)
{
}

void MCWidget::setrect(const MCRectangle& p_rectangle)
{
	rect = p_rectangle;
}

void MCWidget::recompute(void)
{
}

Exec_stat MCWidget::getprop(uint4 p_part_id, Properties p_which, MCExecPoint& p_context, Boolean p_effective)
{
	switch(p_which)
	{
	default:
		break;
	}

	return MCControl::getprop(p_part_id, p_which, p_context, p_effective);
}

Exec_stat MCWidget::setprop(uint4 p_part_id, Properties p_which, MCExecPoint& p_context, Boolean p_effective)
{
	switch(p_which)
	{
	case P_IMPLEMENTATION:
		return SetImplementation(p_context . getsvalue());
	default:
		break;
	}

	return MCControl::setprop(p_part_id, p_which, p_context, p_effective);
}

Exec_stat MCWidget::handle(Handler_type p_type, MCNameRef p_method, MCParameter *p_parameters, MCObject *p_passing_object)
{
	return MCControl::handle(p_type, p_method, p_parameters, p_passing_object);
}

IO_stat MCWidget::load(IO_handle p_stream, const char *version)
{
	return IO_ERROR;
}

IO_stat MCWidget::save(IO_handle p_stream, uint4 p_part, bool p_force_ext)
{
	return IO_ERROR;
}

MCControl *MCWidget::clone(Boolean p_attach, Object_pos p_position, bool invisible)
{
	MCWidget *t_new_widget;
	t_new_widget = new MCWidget(*this);
	if (p_attach)
		t_new_widget -> attach(p_position, invisible);
	return t_new_widget;
}

void MCWidget::draw(MCDC *dc, const MCRectangle& p_dirty, bool p_isolated, bool p_sprite)
{
	MCRectangle dirty;
	dirty = p_dirty;
	
	if (!p_isolated)
	{
		if (!p_sprite)
		{
			dc -> setopacity(blendlevel * 255 / 100);
			dc -> setfunction(ink);
		}
		
		if (m_bitmap_effects == nil)
			dc -> begin(true);
		else
		{
			if (!dc -> begin_with_effects(m_bitmap_effects, MCU_reduce_rect(rect, -gettransient())))
				return;
			dirty = dc -> getclip();
		}
	}

	MCwidgetcontext = dc;
	MCwidgetobject = this;
	MCwidgetcontextopacity = 255;
	OnPaint();
	MCwidgetobject = nil;
	MCwidgetcontext = nil;
	
	/*if (m_self != NULL)
	{
		WidgetEnvironment t_env(this);

		MCRectangle t_dirty_bounds;
		t_dirty_bounds = MCU_intersect_rect(t_bounds, p_dirty);

		MCWidgetRectangle t_dirty;
		t_dirty . left = t_dirty_bounds . x;
		t_dirty . top = t_dirty_bounds . y;
		t_dirty . right = t_dirty_bounds . x + t_dirty_bounds . width;
		t_dirty . bottom = t_dirty_bounds . y + t_dirty_bounds . height;

		if (p_context -> gettype() == CONTEXT_TYPE_PRINTER)
		{
			void *t_pict_dc;
			MCWidgetContextBeginOffscreen(p_context, t_pict_dc);
			m_self -> OnPaint(&t_env, t_pict_dc, t_dirty);
			MCWidgetContextEndOffscreen(p_context, t_pict_dc);
		}
		else
		{

			void *t_native_dc;
			t_native_dc = MCWidgetContextLockNative(p_context);
			m_self -> OnPaint(&t_env, t_native_dc, t_dirty);
			MCWidgetContextUnlockNative(p_context);
		}
	}*/

	if (!p_isolated)
	{
		dc -> end();

		if (getstate(CS_SELECTED))
			drawselected(dc);
	}
}

Boolean MCWidget::maskrect(const MCRectangle& p_rect)
{
	if (!(getflag(F_VISIBLE) || MCshowinvisibles))
		return False;

	MCRectangle drect = MCU_intersect_rect(p_rect, rect);

	return drect.width != 0 && drect.height != 0;
}

////////////////////////////////////////////////////////////////////////////////

void MCWidget::OnOpen(void)
{
}

void MCWidget::OnClose(void)
{
}

void MCWidget::OnFocus(void)
{
}

void MCWidget::OnUnfocus(void)
{
}

void MCWidget::OnMouseEnter(void)
{
}

void MCWidget::OnMouseMove(int32_t x, int32_t y, uint32_t modifiers)
{
}

void MCWidget::OnMouseLeave(void)
{
}

void MCWidget::OnMouseDown(uint32_t button, int32_t x, int32_t y, uint32_t modifiers)
{
}

void MCWidget::OnMouseUp(uint32_t button, int32_t x, int32_t y, uint32_t modifiers)
{
}

bool MCWidget::OnKeyPress(uint32_t key, uint32_t modifiers)
{
	return false;
}

bool MCWidget::OnHitTest(const MCRectangle& region)
{
	return false;
}

void MCWidget::OnPaint(void)
{
	MCAutoNameRef t_handler_name;
	t_handler_name . CreateWithCString("paint");
	
	MCHandler *t_handler;
	if (m_imp_handlers -> findhandler(HT_MESSAGE, t_handler_name, t_handler) != ES_NORMAL)
		return;
	
	Boolean t_old_trace;
	uint2 t_old_breaks;
	t_old_trace = MCtrace;
	t_old_breaks = MCnbreakpoints;
	
	MCtrace = False;
	MCnbreakpoints = 0;
	
	MCRedrawLockScreen();
	
	Exec_stat t_stat;
	MCExecPoint ep(this, m_imp_handlers, t_handler);
	t_stat = t_handler -> exec(ep, nil);

	MCRedrawUnlockScreen();
	
	MCtrace = t_old_trace;
	MCnbreakpoints = t_old_breaks;
}

////////////////////////////////////////////////////////////////////////////////

Exec_stat MCWidget::SetImplementation(const MCString& p_script)
{
	char *t_new_script;
	t_new_script = p_script . clone();
	
	MCHandlerlist *t_new_handlers;
	t_new_handlers = new MCHandlerlist;
	
	Parse_stat t_stat;
	t_stat = t_new_handlers -> parse(this, t_new_script);
	if (t_stat != PS_NORMAL)
	{
		delete t_new_script;
		delete t_new_handlers;
		return ES_ERROR;
	}
	
	delete m_imp_script;
	delete m_imp_handlers;
	
	m_imp_script = t_new_script;
	m_imp_handlers = t_new_handlers;
	
	layer_redrawall();
	
	return ES_NORMAL;
}

////////////////////////////////////////////////////////////////////////////////

Exec_stat MCWidgetCanvasSetColor(MCExecPoint& ep, const MCColor& p_color)
{
	if (MCwidgetcontext == nil)
		return ES_NORMAL;
	
	MCwidgetcontext -> setfillstyle(FillSolid, DNULL, 0, 0);
	MCwidgetcontext -> setforeground(p_color);
	
	return ES_NORMAL;
}

Exec_stat MCWidgetCanvasSetFont(MCExecPoint& ep, MCFontRef p_font)
{
	return ES_NORMAL;
}

Exec_stat MCWidgetCanvasSetOpacity(MCExecPoint& ep, uint32_t p_opacity)
{
	if (MCwidgetcontext == nil)
		return ES_NORMAL;
	
	return ES_NORMAL;
}

Exec_stat MCWidgetCanvasDrawRectangle(MCExecPoint& ep, const MCRectangle& p_rectangle)
{
	if (MCwidgetcontext == nil)
		return ES_NORMAL;
	
	MCwidgetcontext -> drawrect(MCU_offset_rect(p_rectangle, MCwidgetobject -> getrect() . x, MCwidgetobject -> getrect() . y));
	
	return ES_NORMAL;
}

Exec_stat MCWidgetCanvasFillRectangle(MCExecPoint& ep, const MCRectangle& p_rectangle)
{	
	if (MCwidgetcontext == nil)
		return ES_NORMAL;
	
	MCwidgetcontext -> fillrect(MCU_offset_rect(p_rectangle, MCwidgetobject -> getrect() . x, MCwidgetobject -> getrect() . y));
	
	return ES_NORMAL;
}

////////////////////////////////////////////////////////////////////////////////

class MCWidgetCanvasSetCmd: public MCStatement
{
public:
	MCWidgetCanvasSetCmd(void)
	{
		m_value = nil;
	}
	
	~MCWidgetCanvasSetCmd(void)
	{
		delete m_value;
	}
	
	Parse_stat parse(MCScriptPoint& sp)
	{
		initpoint(sp);
		
		if (sp . parseexp(False, True, &m_value) != PS_NORMAL)
			return PS_ERROR;
		
		return PS_NORMAL;
	}
	
protected:
	MCExpression *m_value;
};

// widget canvas set color <color>
class MCWidgetCanvasSetColorCmd: public MCWidgetCanvasSetCmd
{
public:
	Exec_stat exec(MCExecPoint& ep)
	{
		Exec_stat t_stat;
		t_stat = ES_NORMAL;
		
		if (t_stat == ES_NORMAL)
			t_stat = m_value -> eval(ep);
		
		MCColor t_color;
		if (t_stat == ES_NORMAL)
		{
			if (!MCscreen -> parsecolor(ep . getsvalue(), &t_color, nil))
				t_stat = ES_ERROR;
		}
		
		if (t_stat == ES_NORMAL)
			t_stat = MCWidgetCanvasSetColor(ep, t_color);
		
		return t_stat;
	}
};

// widget canvas set opacity tOpacity
class MCWidgetCanvasSetOpacityCmd: public MCWidgetCanvasSetCmd
{
public:
	Exec_stat exec(MCExecPoint& ep)
	{
		Exec_stat t_stat;
		t_stat = ES_NORMAL;
		
		if (t_stat == ES_NORMAL)
			t_stat = m_value -> eval(ep);
		
		if (t_stat == ES_NORMAL)
			t_stat = ep . ston();
		
		if (t_stat == ES_NORMAL)
			t_stat = MCWidgetCanvasSetOpacity(ep, MCU_min(MCU_max(0, ep . getint4()), 255));
		
		return t_stat;
	}
};

// widget canvas set font tFont
class MCWidgetCanvasSetFontCmd: public MCWidgetCanvasSetCmd
{
public:
	Exec_stat exec(MCExecPoint& ep)
	{
		return ES_NORMAL;
	}
};

// widget canvas set stroke tStroke
class MCWidgetCanvasSetStrokeCmd: public MCWidgetCanvasSetCmd
{
public:
	Exec_stat exec(MCExecPoint& ep)
	{
		return ES_NORMAL;
	}
};

// widget canvas ( draw | fill ) rectangle <rectangle>
class MCWidgetCanvasDrawOrFillRectangleCmd: public MCStatement
{
public:
	MCWidgetCanvasDrawOrFillRectangleCmd(bool is_draw)
	{
		m_is_draw = is_draw;
		m_rectangle = nil;
	}
	
	~MCWidgetCanvasDrawOrFillRectangleCmd(void)
	{
		delete m_rectangle;
	}
	
	Parse_stat parse(MCScriptPoint& sp)
	{
		if (sp . parseexp(False, True, &m_rectangle) != PS_NORMAL)
			return PS_ERROR;
		
		return PS_NORMAL;
	}
	
	Exec_stat exec(MCExecPoint& ep)
	{
		if (m_rectangle -> eval(ep) != ES_NORMAL)
			return ES_ERROR;
		
		int16_t t_left, t_top, t_right, t_bottom;
		if (!MCU_stoi2x4(ep . getsvalue(), t_left, t_top, t_right, t_bottom))
			return ES_ERROR;
		
		MCRectangle t_rect;
		MCU_set_rect(t_rect, t_left, t_top, t_right - t_left, t_bottom - t_top);
		
		if (m_is_draw)
			MCWidgetCanvasDrawRectangle(ep, t_rect);
		else
			MCWidgetCanvasFillRectangle(ep, t_rect);
		
		return ES_NORMAL;
	}
	
private:
	bool m_is_draw : 1;
	MCExpression *m_rectangle;
};

class MCWidgetCanvasDrawRectangleCmd: public MCWidgetCanvasDrawOrFillRectangleCmd
{
public:
	MCWidgetCanvasDrawRectangleCmd(void)
		: MCWidgetCanvasDrawOrFillRectangleCmd(true)
	{
	}
};

class MCWidgetCanvasFillRectangleCmd: public MCWidgetCanvasDrawOrFillRectangleCmd
{
public:
	MCWidgetCanvasFillRectangleCmd(void)
		: MCWidgetCanvasDrawOrFillRectangleCmd(false)
	{
	}
};

// widget canvas ( draw | fill ) polygon tPoints
// widget canvas ( draw | fill ) path tPath [ with transform tTransform ]
// widget canvas fill [ unicode ] text tText at <point>
// widget canvas measure [ unicode ] text tText [ into <var> ]

struct MCWidgetVerb
{
	const char *tokens;
	MCStatement *(*factory)(void);
};

template<class T> inline MCStatement *class_factory(void) { return new T; }
static MCWidgetVerb s_widget_verbs[] =
{
	{ "canvas set color", class_factory<MCWidgetCanvasSetColorCmd> },
	{ "canvas set opacity", class_factory<MCWidgetCanvasSetOpacityCmd> },
	{ "canvas set font", class_factory<MCWidgetCanvasSetFontCmd> },
	{ "canvas set stroke", class_factory<MCWidgetCanvasSetStrokeCmd> },
	{ "canvas draw rectangle", class_factory<MCWidgetCanvasDrawRectangleCmd> },
	{ "canvas fill rectangle", class_factory<MCWidgetCanvasFillRectangleCmd> },
	{ "canvas draw polygon", nil },
	{ "canvas fill polygon", nil },
	{ "canvas draw path", nil },
	{ "canvas fill path", nil },
	{ "canvas fill text", nil },
	{ "canvas fill unicode text", nil },
	{ "canvas measure text", nil },
	{ "canvas measure unicode text", nil },
};

MCWidgetCmd::MCWidgetCmd(void)
{
	m_statement = nil;
}

MCWidgetCmd::~MCWidgetCmd(void)
{
	delete m_statement;
}

Parse_stat MCWidgetCmd::parse(MCScriptPoint& sp)
{
	Symbol_type t_type;
	
	initpoint(sp);
	
	for(uindex_t i = 0; i < sizeof(s_widget_verbs) / sizeof(s_widget_verbs[0]); i++)
	{
		MCScriptPoint old_sp(sp);
		
		bool t_matched;
		t_matched = true;
		
		const char *t_tokens;
		t_tokens = s_widget_verbs[i] . tokens;
		while(*t_tokens != '\0')
		{
			const char *t_token_end;
			t_token_end = strchr(t_tokens, ' ');
			if (t_token_end == nil)
				t_token_end = t_tokens + strlen(t_tokens);
			
			Symbol_type t_type;
			if (sp . next(t_type) != PS_NORMAL || t_type != ST_ID)
			{
				t_matched = false;
				break;
			}
			
			MCString t_token(t_tokens, t_token_end - t_tokens);
			if (sp . gettoken() != t_token)
			{
				t_matched = false;
				break;
			}
			
			if (*t_token_end != '\0')
				t_token_end += 1;
							 
			t_tokens = t_token_end;
		}
		
		if (t_matched)
		{
			m_statement = s_widget_verbs[i] . factory();
			return m_statement -> parse(sp);
		}
		
		sp = old_sp;
	}
	
	MCperror -> add(PE_INTERNAL_BADVERB, sp);
	return PS_ERROR;
}

Exec_stat MCWidgetCmd::exec(MCExecPoint& ep)
{
	return m_statement -> exec(ep);
}

////////////////////////////////////////////////////////////////////////////////
