#include "keyboard.h"
#include "print.h"
#include "interrupt.h"
#include "io.h"
#include "global.h"


#define KBD_BUF_PORT 0x60

#define esc '\033' 					//8 base
#define backspace '\b'
#define tab '\t'
#define enter '\r'
#define delete '\177'

#define char_invisible 0 
#define ctrl_l_char char_invisible
#define ctrl_r_char char_invisible
#define shift_l_char char_invisible
#define shift_r_char char_invisible
#define alt_l_char char_invisible
#define alt_r_char char_invisible
#define caps_lock_char char_invisible

#define shift_l_make 0x2a
#define shift_r_make 0x36
#define alt_l_make 0x38
#define alt_r_make 0xe038
#define alt_r_break 0xe0b8
#define ctrl_l_make 0x1d
#define ctrl_r_make 0xe01d
#define ctrl_r_break 0xe09d
#define caps_lock_make 0x3a

//record these key is it pressed
static bool ctrl_status ,shift_status , alt_status ,caps_lock_status,ext_scancode;

static char keymap[][2] = {{0, 0},
                           {esc, esc},
                           {'1', '!'},
                           {'2', '@'},
                           {'3', '#'},
                           {'4', '$'},
                           {'5', '%'},
                           {'6', '^'},
                           {'7', '&'},
                           {'8', '*'},
                           {'9', '('},
                           {'0', ')'},
                           {'-', '_'},
                           {'=', '+'},
                           {backspace, backspace},
                           {tab, tab},
                           {'q', 'Q'},
                           {'w', 'W'},
                           {'e', 'E'},
                           {'r', 'R'},
                           {'t', 'T'},
                           {'y', 'Y'},
                           {'u', 'U'},
                           {'i', 'I'},
                           {'o', 'O'},
                           {'p', 'P'},
                           {'[', '{'},
                           {']', '}'},
                           {enter, enter},
                           {ctrl_l_char, ctrl_l_char},
                           {'a', 'A'},
                           {'s', 'S'},
                           {'d', 'D'},
                           {'f', 'F'},
                           {'g', 'G'},
                           {'h', 'H'},
                           {'j', 'J'},
                           {'k', 'K'},
                           {'l', 'L'},
                           {';', ':'},
                           {'\'', '"'},
                           {'`', '~'},
                           {shift_l_char, shift_l_char},
                           {'\\', '|'},
                           {'z', 'Z'},
                           {'x', 'X'},
                           {'c', 'C'},
                           {'v', 'V'},
                           {'b', 'B'},
                           {'n', 'N'},
                           {'m', 'M'},
                           {',', '<'},
                           {'.', '>'},
                           {'/', '?'},
                           {shift_r_char, shift_r_char},
                           {'*', '*'},
                           {alt_l_char, alt_l_char},
                           {' ', ' '},
                           {caps_lock_char, caps_lock_char},
			   //not handle other key temporary
};



//keyboard interrupt handler
static void intr_keyboard_handler(void){
	//judge last of interrupt of these keys was pressed
	bool ctrl_down_last = ctrl_status;
	bool shift_down_last = shift_status;
	bool caps_lock_last = caps_lock_status;

	bool break_code;
	uint16_t scancode = inb(KBD_BUF_PORT);

	//if this time the scancode is start with 'e0',mean that this key will generate many scancode,so stop this interrupt handler , wait for next scancode
	if (scancode == 0xe0){
		ext_scancode = true;
		return ;
	}

	//merge e0 if last time is start with e0
	if (ext_scancode) {
		scancode = (0xe000 | scancode);
		ext_scancode = false;
	}

	break_code = ((scancode &0x0080) != 0);
	
	if (break_code){
		uint16_t make_code =(scancode &= 0xff7f);		//0xff7f = 1111111101111111
		
		if (make_code ==ctrl_l_make || make_code == ctrl_r_make){
			ctrl_status = false;
		}else if(make_code == alt_l_make || make_code == alt_r_make){
			alt_status = false;
		}else if (make_code == shift_l_make ||  make_code == shift_r_make){
			shift_status = false;
		}
		return ;
	}else if ((scancode >0x00 && scancode < 0x3b) || (scancode == ctrl_r_make ) || (scancode == alt_r_make)){
		bool shift = false;
		
		// (< 0x0e) :'0~9' , '-' '='
		// 0x29 :'`'
		// 0x1a :'['
		// 0x1b :']'
		// 0x2b :'\\'
		// 0x27 :';'
		// 0x28 :'\''
		// 0x33 :','
		// 0x34 :'.'
		// 0x35 :'/'
		if ((scancode < 0x0e ) || (scancode ==0x29 ) ||	\
		(scancode ==0x1a ) || (scancode == 0x1b) ||	\
	       	(scancode == 0x2b) || (scancode ==0x27) ||	\
		(scancode == 0x28 ) || (scancode == 0x33) || 	\
		(scancode == 0x34 ) || (scancode == 0x35) ){
			if (shift_down_last){
				shift = true;
			}
		}else {
			//press at the same time
			if (shift_down_last && caps_lock_last){
				shift = false;
			}else if (shift_down_last || caps_lock_last){
				shift = true ;
			}else {
				shift = false;
			}
		}

		uint8_t index = (scancode &= 0x00ff );

		char cur_char = keymap[index][shift];

		if (cur_char){
			put_char(cur_char);
			return ;
		}
		if (scancode == ctrl_l_make || scancode == ctrl_r_make){
			ctrl_status = true;
		}else if (scancode == shift_l_make || scancode == shift_r_make){
			shift_status = true;
		}else if (scancode == alt_l_make || scancode == alt_r_make){
			alt_status = true;
		}else if (scancode == caps_lock_make){
			caps_lock_status = !caps_lock_status;
		}
	}else {
		put_str("unknown key \n");

	}
}

	



//initial keyboard
void keyboard_init(){
	put_str("keyboard init start\n");
	register_handler(0x21,intr_keyboard_handler);
	put_str("keyboard init done\n");
}
