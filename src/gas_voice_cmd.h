/*
 Date: 2018/09/17
 
 This is Google assistant customization action response process Class

*/


#include <string>
#include <thread>
#include <iostream>
#include <fstream>
#include <sstream>

class GoogleAssistant_voice_cmd {

public:
    GoogleAssistant_voice_cmd();
    ~GoogleAssistant_voice_cmd();
	
    int music_control(char *in_cmd);	
 


};
