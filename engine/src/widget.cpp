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
#include "font.h"
#include "chunk.h"

#include "globals.h"
#include "context.h"

////////////////////////////////////////////////////////////////////////////////

static MCContext *MCwidgetcontext;
static MCRectangle MCwidgetcontextclip;
static MCFontRef MCwidgetcontextfont;
static MCWidget *MCwidgetobject;

////////////////////////////////////////////////////////////////////////////////

MCWidget::MCWidget(void)
{
	m_imp_script = nil;
	m_imp_handlers = nil;
	
	m_mouse_over = false;
	m_mouse_x = INT32_MIN;
	m_mouse_y = INT32_MAX;
	m_button_state = 0;
	m_modifier_state = 0;
}

MCWidget::MCWidget(const MCWidget& p_other)
	: MCControl(p_other)
{
	m_imp_script = nil;
	m_imp_handlers = nil;
	
	m_mouse_over = false;
	m_mouse_x = INT32_MIN;
	m_mouse_y = INT32_MAX;
	m_button_state = 0;
	m_modifier_state = 0;
	
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
	if (state & CS_MENU_ATTACHED)
		return MCObject::mdown(p_which);
	
	if ((m_button_state & (1 << p_which)) != 0)
		return True;

	switch(getstack() -> gettool(this))
	{
	case T_BROWSE:
		setstate(True, CS_MFOCUSED);
		m_button_state |= 1 << p_which;
		OnMouseDown(p_which);
		break;

	case T_POINTER:
		if (getstate(CS_MFOCUSED))
			return False;
		setstate(True, CS_MFOCUSED);
		if (p_which == Button1)
			start(True);
		break;

	default:
		break;	
	}

	return True;
}

Boolean MCWidget::mup(uint2 p_which)
{
	if (state & CS_MENU_ATTACHED)
		return MCObject::mup(p_which);
	
	switch(getstack() -> gettool(this))
	{
	case T_BROWSE:
		m_button_state &= ~(1 << p_which);
		if (m_button_state == 0)
			setstate(False, CS_MFOCUSED);
		if (maskrect(MCU_make_rect(mx, my, 1, 1)))	
			OnMouseUp(p_which);
		else
			OnMouseRelease(p_which);
		break;

	case T_POINTER:
		if (!getstate(CS_MFOCUSED))
			return False;
		setstate(False, CS_MFOCUSED);
		if (p_which == Button1)
			end();
		break;
			
	default:
		break;	

	}

	return True;
}

Boolean MCWidget::mfocus(int2 p_x, int2 p_y)
{
	if (!(getflag(F_VISIBLE) || MCshowinvisibles) ||
		(getflag(F_DISABLED) && (getstack() -> gettool(this) == T_BROWSE)))
		return False;
	
	if (getstack() -> gettool(this) != T_BROWSE)
		return MCControl::mfocus(p_x, p_y);
	
	if (m_button_state == 0 && !maskrect(MCU_make_rect(p_x, p_y, 1, 1)))
		return False;
	
	// Update the mouse loc.
	mx = p_x;
	my = p_y;
	
	// Get control local coords
	int32_t t_mouse_x, t_mouse_y;
	t_mouse_x = p_x - getrect() . x;
	t_mouse_y = p_y - getrect() . y;
	
	// Check to see if pos has changed
	bool t_pos_changed;
	t_pos_changed = false;
	if (t_mouse_x != m_mouse_x || t_mouse_y != m_mouse_y)
	{
		m_mouse_x = t_mouse_x;
		m_mouse_y = t_mouse_y;
		t_pos_changed = true;
	}
		
	// If we weren't previously under the mouse, we are now.
	if (!m_mouse_over)
	{
		m_mouse_over = true;
		OnMouseEnter();
	}
	
	// Dispatch a position update if needed.
	if (t_pos_changed)
		OnMouseMove(t_mouse_x, t_mouse_y);
	
	return True;
}

void MCWidget::munfocus(void)
{
	if (getstack() -> gettool(this) != T_BROWSE ||
		(!m_mouse_over && m_button_state == 0))
	{
		MCControl::munfocus();
		return;
	}
	
	if (m_button_state != 0)
	{
		for(int32_t i = 0; i < 3; i++)
			if ((m_button_state & (1 << i)) != 0)
			{
				m_button_state &= ~(1 << i);
				OnMouseRelease(i);
			}
	}
	
	m_mouse_over = false;
	OnMouseLeave();
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
	MCRectangle t_old_rect;
	t_old_rect = rect;
	
	rect = p_rectangle;
	
	OnReshape(t_old_rect);
}

void MCWidget::recompute(void)
{
}

Exec_stat MCWidget::getprop(uint4 p_part_id, Properties p_which, MCExecPoint& p_context, Boolean p_effective)
{
	// If we are getting any of the reserved properties, then pass directly
	// to MCControl (and super-classes) to handle. Any changes in these will
	// be notified to us so we can take action - but widget's have no direct
	// control over them.
	switch(p_which)
	{
		case P_ID:
		case P_SHORT_ID:	
		case P_LONG_ID:
		case P_ABBREV_ID:
		case P_NAME:
		case P_SHORT_NAME:
		case P_ABBREV_NAME:
		case P_LONG_NAME:
		case P_ALT_ID:
		case P_LAYER:
		case P_SCRIPT:
		case P_PARENT_SCRIPT:
		case P_NUMBER:
/*		case P_FORE_PIXEL:
		case P_BACK_PIXEL:
		case P_HILITE_PIXEL:
		case P_BORDER_PIXEL:
		case P_TOP_PIXEL:
		case P_BOTTOM_PIXEL:
		case P_SHADOW_PIXEL:
		case P_FOCUS_PIXEL:
		case P_PEN_COLOR:
		case P_BRUSH_COLOR:
		case P_FORE_COLOR:
		case P_BACK_COLOR:
		case P_HILITE_COLOR:
		case P_BORDER_COLOR:
		case P_TOP_COLOR:
		case P_BOTTOM_COLOR:
		case P_SHADOW_COLOR:
		case P_FOCUS_COLOR:
		case P_COLORS:
		case P_FORE_PATTERN:
		case P_BACK_PATTERN:
		case P_HILITE_PATTERN:
		case P_BORDER_PATTERN:
		case P_TOP_PATTERN:
		case P_BOTTOM_PATTERN:
		case P_SHADOW_PATTERN:
		case P_FOCUS_PATTERN:
		case P_PATTERNS:
		case P_TEXT_HEIGHT:
		case P_TEXT_ALIGN:
		case P_TEXT_FONT:
		case P_TEXT_SIZE:
		case P_TEXT_STYLE:*/
		case P_LOCK_LOCATION:
		case P_VISIBLE:
		case P_INVISIBLE:
		case P_SELECTED:
		case P_TRAVERSAL_ON:
		case P_OWNER:
		case P_SHORT_OWNER:
		case P_ABBREV_OWNER:
		case P_LONG_OWNER:
		case P_PROPERTIES:
		case P_CUSTOM_PROPERTY_SET:
		case P_CUSTOM_PROPERTY_SETS:
		case P_INK:
		case P_CANT_SELECT:
		case P_BLEND_LEVEL:
		case P_LOCATION:
		case P_LEFT:
		case P_TOP:
		case P_RIGHT:
		case P_BOTTOM:
		case P_TOP_LEFT:
		case P_TOP_RIGHT:
		case P_BOTTOM_LEFT:
		case P_BOTTOM_RIGHT:
		case P_WIDTH:
		case P_HEIGHT:
		case P_RECTANGLE:
		case P_TOOL_TIP:
		case P_UNICODE_TOOL_TIP:
		case P_LAYER_MODE:
			return MCControl::getprop(p_part_id, p_which, p_context, p_effective);
			
		default:
			break;
	}

	// The property we are looking for is not reserved, so we look for a
	// 'getProp' handler in the implementation.
	if (CallGetProp(p_context, p_which, nil, nil))
		return ES_NORMAL;
	
	return ES_ERROR;
}

Exec_stat MCWidget::getnamedprop(uint4 p_part_id, MCNameRef p_property, MCExecPoint& p_context, MCNameRef p_key, Boolean p_effective)
{
	if (CallGetProp(p_context, P_CUSTOM, p_property, p_key))
		return ES_NORMAL;
		
	return MCControl::getnamedprop(p_part_id, p_property, p_context, p_key, p_effective);
}

Exec_stat MCWidget::getarrayprop(uint4 p_part_id, Properties p_which, MCExecPoint& p_context, MCNameRef p_key, Boolean p_effective)
{
	// If we are getting any of the reserved properties, then pass directly
	// to MCControl (and super-classes) to handle. Any changes in these will
	// be notified to us so we can take action - but widget's have no direct
	// control over them.
	switch(p_which)
	{
		//case P_TEXT_STYLE:
		case P_CUSTOM_KEYS:
		case P_CUSTOM_PROPERTIES:
		case P_BITMAP_EFFECT_DROP_SHADOW:
		case P_BITMAP_EFFECT_INNER_SHADOW:
		case P_BITMAP_EFFECT_OUTER_GLOW:
		case P_BITMAP_EFFECT_INNER_GLOW:
		case P_BITMAP_EFFECT_COLOR_OVERLAY:
			return MCControl::getarrayprop(p_part_id, p_which, p_context, p_key, p_effective);
			
		default:
			break;
	}
	
	// The property we are looking for is not reserved, so we look for a
	// 'setProp' handler in the implementation.
	if (CallGetProp(p_context, p_which, nil, p_key))
		return ES_NORMAL;
	
	return ES_NORMAL;
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

	// The property we are looking for is not reserved, so we look for a
	// 'setProp' handler in the implementation.
	if (CallSetProp(p_context, p_which, nil, nil))
		return ES_NORMAL;
		
	return MCControl::setprop(p_part_id, p_which, p_context, p_effective);
}

Exec_stat MCWidget::setnamedprop(uint4 p_part_id, MCNameRef p_property, MCExecPoint& p_context, MCNameRef p_key, Boolean p_effective)
{
	// The property we are looking for is not reserved, so we look for a
	// 'getProp' handler in the implementation.
	if (CallSetProp(p_context, P_CUSTOM, p_property, p_key))
		return ES_NORMAL;
		
	return MCControl::setnamedprop(p_part_id, p_property, p_context, p_key, p_effective);
}

Exec_stat MCWidget::setarrayprop(uint4 p_part_id, Properties p_which, MCExecPoint& p_context, MCNameRef p_key, Boolean p_effective)
{
	switch(p_which)
	{
		default:
			break;
	}
	
	if (CallSetProp(p_context, p_which, nil, p_key))
		return ES_NORMAL;
	
	return MCControl::setarrayprop(p_part_id, p_which, p_context, p_key, p_effective);
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
	MCwidgetcontextfont = nil;
	MCwidgetcontextclip = p_dirty;
	OnPaint();
	MCFontRelease(MCwidgetcontextfont);
	MCwidgetcontext = nil;
	
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

void MCWidget::OnReshape(const MCRectangle& p_old_rect)
{
	MCExecPoint ep;
	ep . setrectangle(p_old_rect);
	
	MCParameter t_param;
	t_param . set_argument(ep);
	
	CallEvent("reshape", &t_param);
}

void MCWidget::OnFocus(void)
{
}

void MCWidget::OnUnfocus(void)
{
}

void MCWidget::OnMouseEnter(void)
{
	CallEvent("mouseEnter", nil);
}

void MCWidget::OnMouseMove(int32_t x, int32_t y)
{
	MCExecPoint ep;
	MCParameter t_param_1, t_param_2;
	ep . setint(x);
	t_param_1 . set_argument(ep);
	ep . setint(y);
	t_param_2 . set_argument(ep);
	t_param_1 . setnext(&t_param_2);
	CallEvent("mouseMove", &t_param_1);
}

void MCWidget::OnMouseLeave(void)
{
	CallEvent("mouseLeave", nil);
}

void MCWidget::OnMouseDown(uint32_t p_button)
{
	MCExecPoint ep;
	MCParameter t_param;
	ep . setint(p_button);
	t_param . set_argument(ep);
	CallEvent("mouseDown", &t_param);
}

void MCWidget::OnMouseUp(uint32_t p_button)
{
	MCExecPoint ep;
	MCParameter t_param;
	ep . setint(p_button);
	t_param . set_argument(ep);
	CallEvent("mouseUp", &t_param);
}

void MCWidget::OnMouseRelease(uint32_t p_button)
{
	MCExecPoint ep;
	MCParameter t_param;
	ep . setint(p_button);
	t_param . set_argument(ep);
	CallEvent("mouseRelease", &t_param);
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
	CallEvent("paint", nil);
}

////////////////////////////////////////////////////////////////////////////////

bool MCWidget::CallEvent(const char *p_name, MCParameter *p_parameters)
{
	MCAutoNameRef t_handler_name;
	t_handler_name . CreateWithCString(p_name);
	
	MCHandler *t_handler;
	if (m_imp_handlers -> findhandler(HT_MESSAGE, P_UNDEFINED, t_handler_name, t_handler) != ES_NORMAL)
		return false;
	
	Boolean t_old_trace;
	uint2 t_old_breaks;
	t_old_trace = MCtrace;
	//t_old_breaks = MCnbreakpoints;
	
	MCtrace = False;
	//MCnbreakpoints = 0;
	
	MCRedrawLockScreen();
	
	Exec_stat t_stat;
	MCExecPoint ep(this, m_imp_handlers, t_handler);
	ep . setscriptobject(this);
	
	MCWidget *t_old_widget_object;
	t_old_widget_object = MCwidgetobject;
	MCwidgetobject = this;
	t_stat = t_handler -> exec(ep, p_parameters);
	MCwidgetobject = t_old_widget_object;
	
	MCRedrawUnlockScreen();
	
	MCtrace = t_old_trace;
	//MCnbreakpoints = t_old_breaks;
	
	return true;
}

bool MCWidget::CallGetProp(MCExecPoint& ep, Properties p_which, MCNameRef p_property, MCNameRef p_key)
{
	MCHandler *t_handler;
	if (p_which != P_CUSTOM)
	{
		if (m_imp_handlers -> findhandler(HT_GETPROP, p_which, nil, t_handler) != ES_NORMAL)
			return false;
	}
	else
	{
		if (m_imp_handlers -> findhandler(HT_GETPROP, P_UNDEFINED, p_property, t_handler) != ES_NORMAL)
			return false;
	}
	
	MCParameter t_param;
	t_param . setnameref_unsafe_argument(p_key == nil ? kMCEmptyName : p_key);
	
	Boolean t_old_trace;
	uint2 t_old_breaks;
	t_old_trace = MCtrace;
	//t_old_breaks = MCnbreakpoints;
	
	MCtrace = False;
	//MCnbreakpoints = 0;
	
	MCRedrawLockScreen();
	
	Exec_stat t_stat;
	MCExecPoint exec_ep(this, m_imp_handlers, t_handler);
	exec_ep . setscriptobject(this);
	
	MCWidget *t_old_widget_object;
	t_old_widget_object = MCwidgetobject;
	MCwidgetobject = this;
	t_stat = t_handler -> exec(exec_ep, &t_param);
	if (t_stat == ES_NORMAL)
	{
		MCresult -> fetch(ep);
		if (ep.getformat() == VF_STRING || ep.getformat() == VF_BOTH)
			ep.grabsvalue();
		else if (ep.getformat() == VF_ARRAY)
			ep.grabarray();
	}
	MCwidgetobject = t_old_widget_object;
	
	MCRedrawUnlockScreen();
	
	MCtrace = t_old_trace;
	//MCnbreakpoints = t_old_breaks;
	
	return true;	
}

bool MCWidget::CallSetProp(MCExecPoint& ep, Properties p_which, MCNameRef p_property, MCNameRef p_key)
{
	MCHandler *t_handler;
	if (p_which != P_CUSTOM)
	{
		if (m_imp_handlers -> findhandler(HT_SETPROP, p_which, nil, t_handler) != ES_NORMAL)
			return false;
	}
	else
	{
		if (m_imp_handlers -> findhandler(HT_SETPROP, P_UNDEFINED, p_property, t_handler) != ES_NORMAL)
			return false;
	}
	
	MCParameter t_param_1, t_param_2;
	t_param_1 . setnameref_unsafe_argument(p_key == nil ? kMCEmptyName : p_key);
	t_param_2 . set_argument(ep);
	t_param_1 . setnext(&t_param_2);
	
	Boolean t_old_trace;
	uint2 t_old_breaks;
	t_old_trace = MCtrace;
	//t_old_breaks = MCnbreakpoints;
	
	MCtrace = False;
	//MCnbreakpoints = 0;
	
	MCRedrawLockScreen();
	
	Exec_stat t_stat;
	MCExecPoint exec_ep(this, m_imp_handlers, t_handler);
	exec_ep . setscriptobject(this);
	
	MCWidget *t_old_widget_object;
	t_old_widget_object = MCwidgetobject;
	MCwidgetobject = this;
	t_stat = t_handler -> exec(exec_ep, &t_param_1);
	if (t_stat == ES_NORMAL)
	{
		MCresult -> fetch(ep);
		if (ep.getformat() == VF_STRING || ep.getformat() == VF_BOTH)
			ep.grabsvalue();
		else if (ep.getformat() == VF_ARRAY)
			ep.grabarray();
	}
	MCwidgetobject = t_old_widget_object;
	
	MCRedrawUnlockScreen();
	
	MCtrace = t_old_trace;
	//MCnbreakpoints = t_old_breaks;
	
	return true;	
}

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
	
	MCwidgetcontext -> setfillstyle(FillSolid, NULL, 0, 0);
	MCwidgetcontext -> setforeground(p_color);
	
	return ES_NORMAL;
}

Exec_stat MCWidgetCanvasSetFont(MCExecPoint& ep, MCNameRef p_name, int32_t p_size, int32_t p_style)
{
	if (MCwidgetcontext == nil)
		return ES_NORMAL;
	
	if (p_name == nil || p_size == -1 || p_style == -1)
	{
		MCNameRef t_inh_name;
		uint2 t_inh_size;
		uint2 t_inh_style;
		MCwidgetobject -> getfontattsnew(t_inh_name, t_inh_size, t_inh_style);
		if (p_name == nil)
			p_name = t_inh_name;
		if (p_size == -1)
			p_size = t_inh_size;
		if (p_style == -1)
			p_style = t_inh_style;
	}
	
	if (MCwidgetcontextfont != nil)
		MCFontRelease(MCwidgetcontextfont);
	
	/* UNCHECKED */ MCFontCreate(p_name, MCFontStyleFromTextStyle(p_style), p_size, MCwidgetcontextfont);
		
	return ES_NORMAL;
}

Exec_stat MCWidgetCanvasSetOpacity(MCExecPoint& ep, uint32_t p_opacity)
{
	if (MCwidgetcontext == nil)
		return ES_NORMAL;
	
	return ES_NORMAL;
}

Exec_stat MCWidgetCanvasSetClip(MCExecPoint& ep, const MCRectangle& p_rectangle)
{
	if (MCwidgetcontext == nil)
		return ES_NORMAL;

	MCRectangle t_clip;
	t_clip = MCU_intersect_rect(MCwidgetcontextclip, MCU_offset_rect(p_rectangle, MCwidgetobject -> getrect() . x, MCwidgetobject -> getrect() . y));
	
	MCwidgetcontext -> setclip(t_clip);

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

Exec_stat MCWidgetCanvasFillTextAt(MCExecPoint& ep, bool p_is_unicode, const char *p_text, uint32_t p_text_length, MCPoint p_location)
{
	if (MCwidgetcontext == nil)
		return ES_NORMAL;
	
	MCFontRef t_font;
	if (MCwidgetcontextfont != nil)
		t_font = MCwidgetcontextfont;
	else
		t_font = MCwidgetobject -> getfontref();
	
	MCFontDrawText(t_font, p_text, p_text_length, p_is_unicode, MCwidgetcontext, p_location . x + MCwidgetobject -> getrect() . x, p_location . y + MCwidgetobject -> getrect() . y, False); 

	return ES_NORMAL;
}

Exec_stat MCWidgetCanvasFillTextAligned(MCExecPoint& ep, bool p_is_unicode, const char *p_text, uint32_t p_text_length, int p_halign, int p_valign, MCRectangle p_rect)
{
	if (MCwidgetcontext == nil)
		return ES_NORMAL;
	
	MCFontRef t_font;
	if (MCwidgetcontextfont != nil)
		t_font = MCwidgetcontextfont;
	else
		t_font = MCwidgetobject -> getfontref();
	
	int32_t t_text_width;
	t_text_width = MCFontMeasureText(t_font, p_text, p_text_length, p_is_unicode);
	
	int32_t t_x;
	switch(p_halign)
	{
	case -1:
		t_x = 0;
		break;
	case 0:
		t_x = (p_rect . width - t_text_width) / 2;
		break;
	case 1:
		t_x = p_rect . width - t_text_width;
		break;
	default:
		assert(false);
		break;
	}
	
	int32_t t_y;
	switch(p_valign)
	{
	case -1:
		t_y = MCFontGetAscent(t_font);
		break;
	case 0:
		t_y = (p_rect . height - (MCFontGetAscent(t_font) + MCFontGetDescent(t_font))) / 2 + MCFontGetAscent(t_font);
		break;
	case 1:
		t_y = p_rect . height - MCFontGetDescent(t_font);
		break;
	default:
		assert(false);
		break;
	}
	
	MCFontDrawText(t_font, p_text, p_text_length, p_is_unicode, MCwidgetcontext, p_rect . x + t_x + MCwidgetobject -> getrect() . x, p_rect . y + t_y + MCwidgetobject -> getrect() . y, False);
	
	return ES_NORMAL;
}

Exec_stat MCWidgetCanvasMeasureText(MCExecPoint& ep, bool p_is_unicode, const char *p_text, uint32_t p_text_length, MCRectangle& r_bounds)
{
	if (MCwidgetcontext == nil)
		return ES_NORMAL;
	
	MCFontRef t_font;
	if (MCwidgetcontextfont != nil)
		t_font = MCwidgetcontextfont;
	else
		t_font = MCwidgetobject -> getfontref();
	
	r_bounds . x = 0;
	r_bounds . width = MCFontMeasureText(t_font, p_text, p_text_length, p_is_unicode);
	r_bounds . y = -MCFontGetAscent(t_font);
	r_bounds . height = MCFontGetDescent(t_font) + MCFontGetAscent(t_font);
	
	return ES_NORMAL;
}

Exec_stat MCWidgetRedrawAll(MCExecPoint& ep)
{
	if (MCwidgetobject == nil)
		return ES_NORMAL;
	
	MCwidgetobject -> layer_redrawall();
	
	return ES_NORMAL;
}

Exec_stat MCWidgetRedrawRectangle(MCExecPoint& ep, const MCRectangle& p_rectangle)
{
	if (MCwidgetobject == nil)
		return ES_NORMAL;
	
	MCwidgetobject -> layer_redrawrect(MCU_offset_rect(p_rectangle, MCwidgetobject -> getrect() . x, MCwidgetobject -> getrect() . y));
	
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
		Exec_stat t_stat;
		t_stat = ES_NORMAL;
		
		if (t_stat == ES_NORMAL)
			t_stat = m_value -> eval(ep);
		
		if (t_stat == ES_NORMAL && ep . getformat() != VF_ARRAY)
			t_stat = ES_ERROR;
		
		if (t_stat == ES_NORMAL)
		{
			MCExecPoint ep2(ep);
			
			MCAutoNameRef t_name;
			if (ep . getarray() -> fetch_element_if_exists(ep2, "name"))
				ep2 . copyasnameref(t_name);
			int32_t t_size;
			if (ep . getarray() -> fetch_element_if_exists(ep2, "size") && ep2 . ston())
				t_size = ep . getint4();
			else
				t_size = -1;
			uint4 t_flags;
			uint2 t_fheight, t_fsize, t_fstyle;
			char *t_fname;
			int32_t t_style;
			if (ep . getarray() -> fetch_element_if_exists(ep2, "style") &&
				MCF_parsetextatts(P_TEXT_STYLE, ep2 . getsvalue(), t_flags, t_fname, t_fheight, t_fsize, t_fstyle) == ES_NORMAL)
				t_style = t_fstyle;
			else
				t_style = -1;
		
			t_stat = MCWidgetCanvasSetFont(ep, t_name, t_size, t_style);
		}
		
		return t_stat;
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

// widget canvas set clip tRectangle
class MCWidgetCanvasSetClipCmd: public MCWidgetCanvasSetCmd
{
public:
	Exec_stat exec(MCExecPoint& ep)
	{
		if (m_value -> eval(ep) != ES_NORMAL)
			return ES_ERROR;
		
		MCRectangle t_rect;
		if (!ep . copyasrect(t_rect))
			return ES_ERROR;
		
		return MCWidgetCanvasSetClip(ep, t_rect);
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
			return MCWidgetCanvasDrawRectangle(ep, t_rect);
		
		return MCWidgetCanvasFillRectangle(ep, t_rect);
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

// widget canvas fill [ unicode ] text <text> at <point>
// widget canvas fill [ unicode ] text <text> aligned <alignment> in <rect>
class MCWidgetCanvasFillUnicodeOrNativeTextCmd: public MCStatement
{
public:
	MCWidgetCanvasFillUnicodeOrNativeTextCmd(bool p_is_unicode)
	{
		m_is_unicode = p_is_unicode;
		m_text = nil;
		m_location = nil;
		m_alignment = nil;
		m_rectangle = nil;
	}
	
	~MCWidgetCanvasFillUnicodeOrNativeTextCmd(void)
	{
		delete m_text;
		delete m_location;
		delete m_alignment;
		delete m_rectangle;
	}
	
	Parse_stat parse(MCScriptPoint& sp)
	{
		if (sp . parseexp(False, True, &m_text) != PS_NORMAL)
			return PS_ERROR;
		
		if (sp . skip_token(SP_FACTOR, TT_PREP, PT_AT) == PS_NORMAL)
		{
			if (sp . parseexp(False, True, &m_location) != PS_NORMAL)
				return PS_ERROR;
		}
		else if (sp . skip_token(SP_FACTOR, TT_PREP, PT_ALIGNED) == PS_NORMAL)
		{
			 if (sp . parseexp(False, True, &m_alignment) != PS_NORMAL)
				return PS_ERROR;
				
			if (sp . skip_token(SP_FACTOR, TT_IN, PT_IN) != PS_NORMAL)
				return PS_ERROR;
				
			if (sp . parseexp(False, True, &m_rectangle) != PS_NORMAL)
				return PS_ERROR;
		}
		
		return PS_NORMAL;
	}
	
	Exec_stat exec(MCExecPoint& ep)
	{
		if (m_text -> eval(ep) != ES_NORMAL)
			return ES_ERROR;
		
		MCAutoPointer<char> t_buffer;
		uint32_t t_size;
		if (!ep . copyasdata(&t_buffer, t_size))
			return ES_ERROR;
		
		if (m_location != nil)
		{
			if (m_location -> eval(ep) != ES_NORMAL)
				return ES_ERROR;
		
			MCPoint t_location;
			if (!ep . copyaspoint(t_location))
				return ES_ERROR;
		
			return MCWidgetCanvasFillTextAt(ep, m_is_unicode, *t_buffer, t_size, t_location);
		}
		
		if (m_alignment -> eval(ep) != ES_NORMAL)
			return ES_ERROR;
			
		if (ep . tos() != ES_NORMAL)
			return ES_ERROR;
			
		const char *t_align_str;
		t_align_str = ep . getcstring();
		
		MCString t_halign_str, t_valign_str;
		const char *t_align_comma_str;
		t_align_comma_str = strchr(t_align_str, ',');
		if (t_align_comma_str != nil)
		{
			t_halign_str . set(t_align_comma_str, strchr(t_align_comma_str, ',') - t_align_comma_str);
			t_valign_str . set(t_align_comma_str + 1, t_align_str + strlen(t_align_str) - t_align_comma_str - 1);
		}
		else
			t_halign_str = t_align_str;
			
		int t_halign;
		t_halign = 0;
		if (t_halign_str == "left")
			t_halign = -1;
		else if (t_halign_str == "center")
			t_halign = 0;
		else if (t_halign_str == "right")
			t_halign = 1;
			
		int t_valign;
		t_valign = 0;
		if (t_valign_str != nil)
		{
			if (t_valign_str == "top")
				t_valign = -1;
			else if (t_valign_str == "middle")
				t_valign = 0;
			else if (t_valign_str == "bottom")
				t_valign = 1;
		}
		
		if (m_rectangle -> eval(ep) != ES_NORMAL)
			return ES_ERROR;

		MCRectangle t_rect;
		if (ep . copyasrect(t_rect) != ES_NORMAL)
			return ES_ERROR;

		return MCWidgetCanvasFillTextAligned(ep, m_is_unicode, *t_buffer, t_size, t_halign, t_valign, t_rect);
	}
	
private:
	bool m_is_unicode;
	MCExpression *m_text;
	MCExpression *m_location;
	MCExpression *m_alignment;
	MCExpression *m_rectangle;
};

class MCWidgetCanvasFillNativeTextCmd: public MCWidgetCanvasFillUnicodeOrNativeTextCmd
{
public:
	MCWidgetCanvasFillNativeTextCmd(void)
		: MCWidgetCanvasFillUnicodeOrNativeTextCmd(false)
	{
	}
};

class MCWidgetCanvasFillUnicodeTextCmd: public MCWidgetCanvasFillUnicodeOrNativeTextCmd
{
public:
	MCWidgetCanvasFillUnicodeTextCmd(void)
		: MCWidgetCanvasFillUnicodeOrNativeTextCmd(true)
	{
	}
};

class MCWidgetCanvasMeasureUnicodeOrNativeTextCmd: public MCStatement
{
public:
	MCWidgetCanvasMeasureUnicodeOrNativeTextCmd(bool p_is_unicode)
	{
		m_is_unicode = p_is_unicode;
		m_text = nil;
		m_target = nil;
		m_it = nil;
	}
	
	~MCWidgetCanvasMeasureUnicodeOrNativeTextCmd(void)
	{
		delete m_text;
		delete m_target;
		delete m_it;
	}
	
	Parse_stat parse(MCScriptPoint& sp)
	{
		if (sp . parseexp(False, True, &m_text) != PS_NORMAL)
			return PS_ERROR;
		
		if (sp . skip_token(SP_FACTOR, TT_PREP, PT_INTO) == PS_NORMAL)
		{
			m_target = new MCChunk(True);
			if (m_target -> parse(sp, False) != PS_NORMAL)
				return PS_ERROR;
		}
		else
			getit(sp, m_it);
		
		return PS_NORMAL;
	}
	
	Exec_stat exec(MCExecPoint& ep)
	{
		if (m_text -> eval(ep) != ES_NORMAL)
			return ES_ERROR;
		
		MCAutoPointer<char> t_buffer;
		uint32_t t_size;
		if (!ep . copyasdata(&t_buffer, t_size))
			return ES_ERROR;
		
		MCRectangle t_bounds;
		if (MCWidgetCanvasMeasureText(ep, m_is_unicode, *t_buffer, t_size, t_bounds) != ES_NORMAL)
			return ES_ERROR;
		
		ep . setrectangle(t_bounds);
		if (m_target != nil)
			return m_target -> set(ep, PT_INTO);
		
		return m_it -> set(ep, False);
	}
	
private:
	bool m_is_unicode;
	MCExpression *m_text;
	MCChunk *m_target;
	MCVarref *m_it;
};

class MCWidgetCanvasMeasureNativeTextCmd: public MCWidgetCanvasMeasureUnicodeOrNativeTextCmd
{
public:
	MCWidgetCanvasMeasureNativeTextCmd(void)
	: MCWidgetCanvasMeasureUnicodeOrNativeTextCmd(false)
	{
	}
};

class MCWidgetCanvasMeasureUnicodeTextCmd: public MCWidgetCanvasMeasureUnicodeOrNativeTextCmd
{
public:
	MCWidgetCanvasMeasureUnicodeTextCmd(void)
	: MCWidgetCanvasMeasureUnicodeOrNativeTextCmd(true)
	{
	}
};

// widget canvas ( draw | fill ) polygon tPoints
// widget canvas ( draw | fill ) path tPath [ with transform tTransform ]
// widget canvas fill [ unicode ] text tText at <point>
// widget canvas measure [ unicode ] text tText [ into <var> ]

class MCWidgetRedrawAllCmd: public MCStatement
{
public:
	MCWidgetRedrawAllCmd(void)
	{
	}
	
	~MCWidgetRedrawAllCmd(void)
	{
	}
	
	Parse_stat parse(MCScriptPoint& sp)
	{
		initpoint(sp);
		return PS_NORMAL;
	}
	
	Exec_stat exec(MCExecPoint& ep)
	{
		return MCWidgetRedrawAll(ep);
	}
};

class MCWidgetRedrawRectangleCmd: public MCStatement
{
public:
	MCWidgetRedrawRectangleCmd(void)
	{
		m_rectangle = nil;
	}
	
	~MCWidgetRedrawRectangleCmd(void)
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
		
		MCRectangle t_rect;
		if (!ep . copyasrect(t_rect))
			return ES_ERROR;
		
		return MCWidgetRedrawRectangle(ep, t_rect);
	}
	
private:
	MCExpression *m_rectangle;
};

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
	{ "canvas set clip", class_factory<MCWidgetCanvasSetClipCmd> },
	{ "canvas draw rectangle", class_factory<MCWidgetCanvasDrawRectangleCmd> },
	{ "canvas fill rectangle", class_factory<MCWidgetCanvasFillRectangleCmd> },
	{ "canvas draw polygon", nil },
	{ "canvas fill polygon", nil },
	{ "canvas draw path", nil },
	{ "canvas fill path", nil },
	{ "canvas fill text", class_factory<MCWidgetCanvasFillNativeTextCmd> },
	{ "canvas fill unicode text", class_factory<MCWidgetCanvasFillUnicodeTextCmd> },
	{ "canvas measure text", class_factory<MCWidgetCanvasMeasureNativeTextCmd> },
	{ "canvas measure unicode text", class_factory<MCWidgetCanvasMeasureUnicodeTextCmd> },
	{ "redraw all", class_factory<MCWidgetRedrawAllCmd> },
	{ "redraw rectangle", class_factory<MCWidgetRedrawRectangleCmd> },
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

typedef struct MCDialect *MCDialectRef;
void MCDialectCreate(MCDialectRef& r_babel);
void MCDialectDestroy(MCDialectRef babel);
void MCDialectAddRule(MCDialectRef babal, const char *syntax, uindex_t action_id);
bool MCDialectIsValid(MCDialectRef babel);

enum MCWidgetDialectActionType
{
	kMCWidgetDialectActionNone,
	
	kMCWidgetDialectActionWidgetDefinition,
	kMCWidgetDialectActionVariableDefinition,
	kMCWidgetDialectActionPropertyDefinition,
	kMCWidgetDialectActionMethodDefinition,
	kMCWidgetDialectActionParameterDefinition,
	
	kMCWidgetDialectActionBooleanType,
	kMCWidgetDialectActionIntegerType,
	kMCWidgetDialectActionUnsignedIntegerType,
	kMCWidgetDialectActionRealType,
	kMCWidgetDialectActionCharacterType,
	kMCWidgetDialectActionStringType,
	kMCWidgetDialectActionBinaryStringType,
};

static struct { const char *syntax; MCWidgetDialectActionType action;} kMCWidgetDialectPhrases[] =
{
	{ "root: widget NAME [ based on NAME ] ; { <definition> , ; } ; end widget", kMCWidgetDialectActionWidgetDefinition },

	{ "definition: variable NAME [ is <type> ]", kMCWidgetDialectActionVariableDefinition },
	{ "definition: property NAME [ is <type> ] [ getter ID ] [ setter ID ]", kMCWidgetDialectActionPropertyDefinition },
	{ "definition: method ID { <parameter> , ',' } [ returns <type> ] ; { <command> } ; end method", kMCWidgetDialectActionMethodDefinition },
	
	{ "parameter: [ @in | @out | @inout ] NAME [ is <type> ]", kMCWidgetDialectActionParameterDefinition },
	
	{ "type: boolean", kMCWidgetDialectActionBooleanType },
	{ "type: integer [ between INTEGER and INTEGER ]", kMCWidgetDialectActionIntegerType },
	{ "type: unsigned integer [ between INTEGER and INTEGER ]", kMCWidgetDialectActionUnsignedIntegerType },
	{ "type: real [ between INTEGER and INTEGER ]", kMCWidgetDialectActionRealType },
	{ "type: character", kMCWidgetDialectActionCharacterType },
	{ "type: string", kMCWidgetDialectActionStringType },
	{ "type: binary string", kMCWidgetDialectActionBinaryStringType },
};

bool MCWidgetCreateDialect(MCDialectRef& r_dialect)
{
	MCDialectRef t_dialect;
	
	MCDialectCreate(t_dialect);
	for(uindex_t i = 0; i < sizeof(kMCWidgetDialectPhrases) / sizeof(kMCWidgetDialectPhrases[0]); i++)
		MCDialectAddRule(t_dialect, kMCWidgetDialectPhrases[i] . syntax, kMCWidgetDialectPhrases[i] . action);
	
	if (MCDialectIsValid(t_dialect))
	{
		r_dialect = t_dialect;
		return true;
	}
	
	MCDialectDestroy(t_dialect);
	
	return false;					  
}

////////////////////////////////////////////////////////////////////////////////

enum MCDialectSyntaxTokenType
{
	kMCDialectSyntaxTokenTypeNone,

	kMCDialectSyntaxTokenTypeIdentifier,
	kMCDialectSyntaxTokenTypeQuotedIdentifier,
	kMCDialectSyntaxTokenTypeAngledIdentifier,
	kMCDialectSyntaxTokenTypeColon,
	kMCDialectSyntaxTokenTypeComma,
	kMCDialectSyntaxTokenTypeSemicolon,
	kMCDialectSyntaxTokenTypeAt,
	kMCDialectSyntaxTokenTypeBar,
	kMCDialectSyntaxTokenTypeLeftBracket,
	kMCDialectSyntaxTokenTypeRightBracket,
	kMCDialectSyntaxTokenTypeLeftBrace,
	kMCDialectSyntaxTokenTypeRightBrace,
	kMCDialectSyntaxTokenTypeLeftParanthesis,
	kMCDialectSyntaxTokenTypeRightParanthesis,
	kMCDialectSyntaxTokenTypeEnd,
	
	kMCDialectSyntaxTokenTypeError,
};

enum MCDialectSyntaxError
{
};

typedef struct MCDialectState *MCDialectStateRef;
typedef struct MCDialectIdentifier *MCDialectIdentifierRef;

class MCAutoDialectStateRef
{
public:

	MCDialectStateRef Take(void);
	
	MCDialectStateRef operator = (MCDialectStateRef other);
	MCDialectStateRef operator * (void);
	MCDialectStateRef& operator & (void);
};

class MCAutoDialectIdentifierRef
{
public:

	MCDialectIdentifierRef Take(void);
	
	MCDialectIdentifierRef operator = (MCDialectIdentifierRef other);
	MCDialectStateRef operator * (void);
	MCDialectIdentifierRef& operator & (void);
};

// alt_expr:
//   { concat_expr , '|' }
// concat_expr:
//   { factor_expr }
// factor_expr
//   : '[' alt_expr ']'
//   | '(' alt_expr ')'
//   | '{' alt_expr ',' alt_expr '}'
//   | id
//   | quot-id
//   | angle-id
//   | '@' id
//   | ';'

static bool MCDialectSyntaxParseOptional(const char*& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error);
static bool MCDialectSyntaxParseGroup(const char*& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error);
static bool MCDialectSyntaxParseRepetition(const char*& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error);
static bool MCDialectSyntaxParseMarked(const char*& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error);
static bool MCDialectSyntaxParseSeparator(const char*& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error);
static bool MCDialectSyntaxParseFactor(const char*& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error);
static bool MCDialectSyntaxParseConcatenation(const char *& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error);
static bool MCDialectSyntaxParseAlternation(const char *& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error);

static bool MCDialectSyntaxMatchRightBracket(const char *& x_syntax, MCDialectSyntaxError& r_error);
static bool MCDialectSyntaxMatchRightParanthesis(const char *& x_syntax, MCDialectSyntaxError& r_error);
static bool MCDialectSyntaxMatchRightBrace(const char *& x_syntax, MCDialectSyntaxError& r_error);
static bool MCDialectSyntaxMatchColon(const char *& x_syntax, MCDialectSyntaxError& r_error);
static bool MCDialectSyntaxMatchEnd(const char *& x_syntax, MCDialectSyntaxError& r_error);
static bool MCDialectSyntaxMatchIdentifier(const char *& x_syntax, MCDialectIdentifierRef& r_id, MCDialectSyntaxError& r_error);

static bool MCDialectSyntaxSkipComma(const char*& x_syntax, bool& r_skipped, MCDialectSyntaxError& r_error);
static bool MCDialectSyntaxSkipLeftBracket(const char*& x_syntax, bool& r_skipped, MCDialectSyntaxError& r_error);
static bool MCDialectSyntaxSkipLeftParanthesis(const char*& x_syntax, bool& r_skipped, MCDialectSyntaxError& r_error);
static bool MCDialectSyntaxSkipLeftBrace(const char*& x_syntax, bool& r_skipped, MCDialectSyntaxError& r_error);
static bool MCDialectSyntaxSkipAt(const char*& x_syntax, bool& r_skipped, MCDialectSyntaxError& r_error);
static bool MCDialectSyntaxSkipSemicolon(const char*& x_syntax, bool& r_skipped, MCDialectSyntaxError& r_error);
static bool MCDialectSyntaxSkipBar(const char*& x_syntax, bool& r_skipped, MCDialectSyntaxError& r_error);
static bool MCDialectSyntaxSkipAnyIdentifier(const char*& x_syntax, MCDialectSyntaxTokenType& r_type, MCDialectIdentifierRef& r_id, MCDialectSyntaxError& r_error);

static bool MCDialectSyntaxWillMatchConcatEnd(const char*& x_syntax, bool& r_will_match, MCDialectSyntaxError& r_error);

static bool MCDialectSyntaxParseOptional(const char*& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error)
{
	MCAutoDialectStateRef t_state;
	if (!MCDialectSyntaxParseAlternation(x_syntax, &t_state, r_error))
		return false;
		
	if (!MCDialectSyntaxMatchRightBracket(x_syntax, r_error))
		return false;
		
	// Add epsilon to alternation
		
	r_state = t_state . Take();
	
	return true;
}

static bool MCDialectSyntaxParseGroup(const char*& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error)
{
	MCAutoDialectStateRef t_state;
	if (!MCDialectSyntaxParseAlternation(x_syntax, &t_state, r_error))
		return false;
		
	if (!MCDialectSyntaxMatchRightParanthesis(x_syntax, r_error))
		return false;
		
	r_state = t_state . Take();
	
	return true;
}

static bool MCDialectSyntaxParseRepetition(const char*& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error)
{
	MCAutoDialectStateRef t_pattern;
	if (!MCDialectSyntaxParseAlternation(x_syntax, &t_pattern, r_error))
		return false;
	
	MCAutoDialectStateRef t_separator;
	bool t_skipped;
	if (!MCDialectSyntaxSkipComma(x_syntax, t_skipped, r_error))
		if (!MCDialectSyntaxParseAlternation(x_syntax, &t_separator, r_error))
			return false;
	
	if (!MCDialectSyntaxMatchRightBrace(x_syntax, r_error))
		return false;
			
	// Build repetition node
	
	return true;
}

static bool MCDialectSyntaxParseMarked(const char*& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error)
{
	MCAutoDialectIdentifierRef t_identifier;
	if (!MCDialectSyntaxMatchIdentifier(x_syntax, &t_identifier, r_error))
		return false;
	
	// Build @ node
	
	return true;
}

static bool MCDialectSyntaxParseSeparator(const char*& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error)
{
	// Build separator node
	
	return true;
}

static bool MCDialectSyntaxParseFactor(const char*& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error)
{
	bool t_skipped;
	
	if (!MCDialectSyntaxSkipLeftBracket(x_syntax, t_skipped, r_error))
		return false;
	if (t_skipped)
		return MCDialectSyntaxParseOptional(x_syntax, r_state, r_error);
	
	if (!MCDialectSyntaxSkipLeftParanthesis(x_syntax, t_skipped, r_error))
		return false;
	if (t_skipped)
		return MCDialectSyntaxParseGroup(x_syntax, r_state, r_error);
	
	if (!MCDialectSyntaxSkipLeftBrace(x_syntax, t_skipped, r_error))
		return false;
	if (t_skipped)
		return MCDialectSyntaxParseRepetition(x_syntax, r_state, r_error);
	
	if (!MCDialectSyntaxSkipAt(x_syntax, t_skipped, r_error))
		return false;
	if (t_skipped)
		return MCDialectSyntaxParseMarked(x_syntax, r_state, r_error);
	
	if (!MCDialectSyntaxSkipSemicolon(x_syntax, t_skipped, r_error))
		return false;
	if (t_skipped)
		return MCDialectSyntaxParseSeparator(x_syntax, r_state, r_error);
	
	MCDialectSyntaxTokenType t_id_type;
	MCAutoDialectIdentifierRef t_id;
	if (!MCDialectSyntaxSkipAnyIdentifier(x_syntax, t_id_type, &t_id, r_error))
		return false;
	if (t_id_type != kMCDialectSyntaxTokenTypeNone)
	{
		// Build id node
		
		return true;
	}
	
	// Build epsilon node
	
	return true;
}

static bool MCDialectSyntaxParseConcatenation(const char *& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error)
{
	MCAutoDialectStateRef t_state;
	for(;;)
	{
		MCAutoDialectStateRef t_new_state;
		if (!MCDialectSyntaxParseFactor(x_syntax, &t_new_state, r_error))
			return false;
		
		if (*t_state == nil)
			t_state = t_new_state . Take();
		else
		{
			// Build concatenation node
		}
		
		bool t_will_match;
		if (!MCDialectSyntaxWillMatchConcatEnd(x_syntax, t_will_match, r_error))
			return false;
		if (t_will_match)
			break;
	}
	
	r_state = t_state . Take();
	
	return true;
}

static bool MCDialectSyntaxParseAlternation(const char *& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error)
{
	MCAutoDialectStateRef t_state;
	for(;;)
	{
		MCAutoDialectStateRef t_new_state;
		if (!MCDialectSyntaxParseConcatenation(x_syntax, &t_state, r_error))
			return false;
		
		if (*t_state == nil)
			t_state = t_new_state . Take();
		else
		{
			// Build alternation node
		}
		
		bool t_skipped;
		if (!MCDialectSyntaxSkipBar(x_syntax, t_skipped, r_error))
			return false;
		
		if (!t_skipped)
			break;
	}
	
	r_state = t_state . Take();
	
	return true;
}

static bool MCDialectSyntaxParse(const char*& x_syntax, MCDialectIdentifierRef& r_scope, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error)
{
	MCAutoDialectIdentifierRef t_scope;
	if (!MCDialectSyntaxMatchIdentifier(x_syntax, &t_scope, r_error))
		return false;
	
	if (!MCDialectSyntaxMatchColon(x_syntax, r_error))
		return false;
	
	MCAutoDialectStateRef t_state;
	if (!MCDialectSyntaxParseAlternation(x_syntax, &t_state, r_error))
		return false;
	
	if (!MCDialectSyntaxMatchEnd(x_syntax, r_error))
		return false;
	
	r_scope = t_scope . Take();
	r_state = t_state . Take();
	
	return true;
}

void MCDialectCreate(MCDialectRef& r_dialect)
{
}

void MCDialectDestroy(MCDialectRef self)
{
}

bool MCDialectIsValid(MCDialectRef self)
{
	return false;
}

void MCDialectAddRule(MCDialectRef self, const char *p_syntax, uindex_t p_action_id)
{
}

////////////////////////////////////////////////////////////////////////////////
