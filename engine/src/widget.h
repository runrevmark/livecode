#ifndef __MC_WIDGET__
#define __MC_WIDGET__

#ifndef __MC_CONTROL__
#include "control.h"
#endif

class MCWidget: public MCControl
{
public:
	MCWidget(void);
	MCWidget(const MCWidget& p_other);
	virtual ~MCWidget(void);

	virtual Chunk_term gettype(void) const;
	virtual const char *gettypestring(void);

	virtual void open(void);
	virtual void close(void);

	virtual void kfocus(void);
	virtual void kunfocus(void);
	virtual Boolean kdown(const char *p_key_string, KeySym p_key);
	virtual Boolean kup(const char *p_key_string, KeySym p_key);

	virtual Boolean mdown(uint2 p_which);
	virtual Boolean mup(uint2 p_which);
	virtual Boolean mfocus(int2 p_x, int2 p_y);
	virtual void munfocus(void);

	virtual Boolean doubledown(uint2 p_which);
	virtual Boolean doubleup(uint2 p_which);
	
	virtual void timer(MCNameRef p_message, MCParameter *p_parameters);

	virtual void setrect(const MCRectangle& p_rectangle);
	virtual void recompute(void);

	virtual Exec_stat getprop(uint4 part_id, Properties which, MCExecPoint& context, Boolean effective);
	virtual Exec_stat getnamedprop(uint4 parid, MCNameRef property, MCExecPoint &, MCNameRef key, Boolean effective);
	virtual Exec_stat getarrayprop(uint4 parid, Properties which, MCExecPoint &, MCNameRef key, Boolean effective);
	virtual Exec_stat setprop(uint4 part_id, Properties which, MCExecPoint& context, Boolean effective);
	virtual Exec_stat setnamedprop(uint4 parid, MCNameRef property, MCExecPoint&, MCNameRef key, Boolean effective);
	virtual Exec_stat setarrayprop(uint4 parid, Properties which, MCExecPoint&, MCNameRef key, Boolean effective);
	
	virtual Exec_stat handle(Handler_type, MCNameRef, MCParameter *, MCObject *pass_from);

	virtual IO_stat save(IO_handle stream, uint4 p_part, bool p_force_ext);
	virtual IO_stat load(IO_handle stream, const char *version);

	virtual MCControl *clone(Boolean p_attach, Object_pos p_position, bool invisible);

	virtual void draw(MCDC *p_dc, const MCRectangle& p_dirty, bool p_isolated, bool p_sprite);
	virtual Boolean maskrect(const MCRectangle& p_rect);
	
private:
	void OnOpen(void);
	void OnClose(void);
	
	void OnReshape(const MCRectangle& old_rect);
	
	void OnFocus(void);
	void OnUnfocus(void);
	
	void OnMouseEnter(void);
	void OnMouseMove(int32_t x, int32_t y);
	void OnMouseLeave(void);
	
	void OnMouseDown(uint32_t button);
	void OnMouseUp(uint32_t button);
	void OnMouseRelease(uint32_t button);
	
	bool OnKeyPress(uint32_t key, uint32_t modifiers);
	
	bool OnHitTest(const MCRectangle& region);
	
	void OnPaint(void);

	//////////
	
	bool CallEvent(const char *name, MCParameter *parameters);
	bool CallGetProp(MCExecPoint& ep, Properties p_property, MCNameRef p_property_name, MCNameRef p_key);
	bool CallSetProp(MCExecPoint& ep, Properties p_property, MCNameRef p_property_name, MCNameRef p_key);
	
	Exec_stat SetImplementation(const MCString& p_script);
	
	//////////
	
	char *m_imp_script;
	MCHandlerlist *m_imp_handlers;
	
	//////////
	
	int32_t m_modifier_state;
	int32_t m_button_state;
	bool m_mouse_over;
	int32_t m_mouse_x, m_mouse_y;
};

#endif
