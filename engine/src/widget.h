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

	virtual Exec_stat getprop(uint4 p_part_id, Properties p_which, MCExecPoint& p_context, Boolean p_effective);
	virtual Exec_stat setprop(uint4 p_part_id, Properties p_which, MCExecPoint& p_context, Boolean p_effective);
	virtual Exec_stat handle(Handler_type, MCNameRef, MCParameter *, MCObject *pass_from);

	virtual IO_stat save(IO_handle stream, uint4 p_part, bool p_force_ext);
	virtual IO_stat load(IO_handle stream, const char *version);

	virtual MCControl *clone(Boolean p_attach, Object_pos p_position, bool invisible);

	virtual void draw(MCDC *p_dc, const MCRectangle& p_dirty, bool p_isolated, bool p_sprite);
	virtual Boolean maskrect(const MCRectangle& p_rect);

protected:
	void OnOpen(void);
	void OnClose(void);
	
	void OnFocus(void);
	void OnUnfocus(void);
	
	void OnMouseEnter(void);
	void OnMouseMove(int32_t x, int32_t y, uint32_t modifiers);
	void OnMouseLeave(void);
	
	void OnMouseDown(uint32_t button, int32_t x, int32_t y, uint32_t modifiers);
	void OnMouseUp(uint32_t button, int32_t x, int32_t y, uint32_t modifiers);
	
	bool OnKeyPress(uint32_t key, uint32_t modifiers);
	
	bool OnHitTest(const MCRectangle& region);
	void OnPaint(void);

private:
	Exec_stat SetImplementation(const MCString& p_script);
	
	char *m_imp_script;
	MCHandlerlist *m_imp_handlers;
};

#endif
