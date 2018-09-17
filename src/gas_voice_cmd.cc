
#include "gas_voice_cmd.h"


GoogleAssistant_voice_cmd::GoogleAssistant_voice_cmd(){
    std::clog << "GoogleAssistant_voice_cmd::GoogleAssistant_voice_cmd::constructer" << std::endl;
}

GoogleAssistant_voice_cmd::~GoogleAssistant_voice_cmd(){
    std::clog << "GoogleAssistant_voice_cmd::GoogleAssistant_voice_cmd::disstructer" << std::endl;
}

int GoogleAssistant_voice_cmd::music_control(char *in_cmd){
    std::clog << "GoogleAssistant_voice_cmd::music control" << std::endl;
	std::clog << "GoogleAssistant_voice_cmd::music command is "<< in_cmd << std::endl;
	return 0;
}	
