/*
Copyright 2017 Google Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <grpc++/grpc++.h>

#include <getopt.h>

#include <chrono>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <iostream>
#include <fstream>
#include <sstream>

#ifdef __linux__
#define ENABLE_ALSA
#endif

#ifdef ENABLE_ALSA
#include "audio_input_alsa.h"
#include "audio_output_alsa.h"
#endif

#include "google/assistant/embedded/v1alpha2/embedded_assistant.pb.h"
#include "google/assistant/embedded/v1alpha2/embedded_assistant.grpc.pb.h"

#include "assistant_config.h"
#include "audio_input.h"
#include "audio_input_file.h"
#include "base64_encode.h"
#include "json_util.h"
#include "gas_voice_cmd.h"

/*** json-c lib ****/
#include <iostream>

//using json_c lib for parse google assistant response json content.
#define JSON_C 1
#define GAS_DEBUG 1

#define CMD_NUMBERS      7
#define FLASHLIGHT       "com.pegatron.commands.FlashLight"
#define DOORCONTROL      "com.pegatron.commands.DoorLookControl"
#define MUSICCONTROL     "com.pegatron.commands.MusicControl"
#define DISTURB          "com.pegatron.commands.DisturbControl"
#define VOLUME           "com.pegatron.commands.VolumeControl"
#define VOLUMEVALUE      "com.pegatron.commands.VolumeValueControl"
#define PHONECALL        "com.pegatron.commands.PhoneCallControl"




#if JSON_C
extern "C" {
// Get declaration for f(int i, char c, float x)
//#include "my-C-code.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "/media/pi/4dbf987b-6d76-40bf-b87a-7e2f0abe2d0f/json-c/json.h"
}

struct json_object *root, *requestId, *inputs,  *inputs_obj, *inputs_obj_name, *intent, *payload;
struct json_object *commands, *commands_obj, *devices, *execution, *execution_obj, *command, *params;
//struct json_object *doorlockaction; //for testing
struct json_object *action_param1,*action_param2;
int arraylen;
int i;
// this json_string is for testing
//char json_string[2048]="{\"requestId\":\"5b92a8e8-0000-26de-a86a-089e082f5c04\",\"inputs\":[{\"intent\":\"action.devices.EXECUTE\",\"payload\":{\"commands\":[{\"devices\":[{\"id\":\"default\"}],\"execution\":[{\"command\":\"com.pegatron.commands.DoorLookControl\",\"params\":{\"doorlockaction\":\"UNLOCK\"}}]}]}}]}";
int json_c_parser(char *in_string,char **gas_command_array,char *out_param1,char *out_param2);
char g_res_json_msg[2048];
int json_c_ret = -1;
char out_param_1[32];
char out_param_2[32];
char out_param_cmd[64];
char gas_response_string[2048];
int not_get_okgoogle_flag = 0;
int  gas_command_count = CMD_NUMBERS;
char *gas_command_array[] = {
                                FLASHLIGHT,
                                DOORCONTROL, 
                                MUSICCONTROL,
                                DISTURB,
                                VOLUME,      
                                VOLUMEVALUE, 								
                                PHONECALL
                            };
#endif

/*** json-c lib ****/

using google::assistant::embedded::v1alpha2::EmbeddedAssistant;
using google::assistant::embedded::v1alpha2::AssistRequest;
using google::assistant::embedded::v1alpha2::AssistResponse;
using google::assistant::embedded::v1alpha2::AudioInConfig;
using google::assistant::embedded::v1alpha2::AudioOutConfig;
using google::assistant::embedded::v1alpha2::ScreenOutConfig;
using google::assistant::embedded::v1alpha2::AssistResponse_EventType_END_OF_UTTERANCE;
using google::assistant::embedded::v1alpha2::DeviceAction;

using grpc::CallCredentials;
using grpc::Channel;
using grpc::ClientReaderWriter;

//PEGA
//using json = nlohmann::json;
  
static const std::string kCredentialsTypeUserAccount = "USER_ACCOUNT";
static const std::string kALSAAudioInput = "ALSA_INPUT";
static const std::string kLanguageCode = "en-US";
static const std::string kDeviceModelId = "default";
static const std::string kDeviceInstanceId = "default";

//bool verbose = false;
bool verbose = true; //PEGA


// Creates a channel to be connected to Google.
std::shared_ptr<Channel> CreateChannel(const std::string& host) {
  std::ifstream file("robots.pem");
  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string roots_pem = buffer.str();

  if (verbose) {
    std::clog << "assistant_sdk robots_pem: " << roots_pem << std::endl;
  }
  ::grpc::SslCredentialsOptions ssl_opts = {roots_pem, "", ""};
  auto creds = ::grpc::SslCredentials(ssl_opts);
  std::string server = host + ":443";
  if (verbose) {
    std::clog << "assistant_sdk CreateCustomChannel(" << server << ", creds, arg)"
      << std::endl << std::endl;
  }
  ::grpc::ChannelArguments channel_args;
  return CreateCustomChannel(server, creds, channel_args);
}

void PrintUsage() {
  std::cerr << "Usage: ./run_assistant_audio "
            << "--credentials <credentials_file> "
            << "[--api_endpoint <API endpoint>] "
            << "[--locale <locale>]"
            << "[--html_out <command to load HTML page>]"
            << std::endl;
}

bool GetCommandLineFlags(
    int argc, char** argv, std::string* credentials_file_path,
    std::string* api_endpoint, std::string* locale, std::string* html_out_command) {
  const struct option long_options[] = {
    {"credentials", required_argument, nullptr, 'c'},
    {"api_endpoint",     required_argument, nullptr, 'e'},
    {"locale",           required_argument, nullptr, 'l'},
    {"verbose",          no_argument, nullptr, 'v'},
    {"html_out",         required_argument, nullptr, 'h'},
    {nullptr, 0, nullptr, 0}
  };
  *api_endpoint = ASSISTANT_ENDPOINT;
  while (true) {
    int option_index;
    int option_char =
        getopt_long(argc, argv, "c:e:l:v:h", long_options, &option_index);
    if (option_char == -1) {
      break;
    }
    switch (option_char) {
      case 'c':
        *credentials_file_path = optarg;
        break;
      case 'e':
        *api_endpoint = optarg;
        break;
      case 'l':
        *locale = optarg;
        break;
      case 'v':
        verbose = true;
        break;
      case 'h':
        *html_out_command = optarg;
        break;
      default:
        PrintUsage();
        return false;
    }
  }
  return true;
}

/*PEGA */
#if JSON_C

  /* json c code process */ 
  /* This is google assistant device action response using json-c lib parser source */
  
//int json_c_parser(char *in_string,char *in_param1,char *in_param2,char *out_param1,char *out_param2)

int json_c_parser(char *in_string,char **gas_command_array,char *out_param1,char *out_param2)
    {
  	//root = json_tokener_parse(json_string);// for testing
  	root = json_tokener_parse(in_string);
	
  	if (root == NULL) {
          printf("google json res parse failed.");
          return -1;
    }
  	requestId = json_object_object_get(root, "requestId");
    inputs = json_object_object_get(root, "inputs");
  	
	if ((requestId == NULL) || (inputs == NULL)){
  	    printf("google json res requestId is NULL or inputs is NULL \n");
		return -1;
    }
	else{
  	    //printf("requestId ==> %s\n", json_object_get_string(requestId));
  	    printf("google json res inputs ==> %s\n", json_object_get_string(inputs));
  	}
	
  	// inputs is an array of objects
    arraylen = json_object_array_length(inputs);
  	//printf("\n>>google json res arraylen= %d\n",arraylen);
  	for (i = 0; i < arraylen; i++) {
        inputs_obj = json_object_array_get_idx(inputs, i);
        // get the name attribute in the i-th object
        intent = json_object_object_get(inputs_obj, "intent");
        // print out the name attribute
        //printf("google json res intent ==> %s\n\n", json_object_get_string(intent));
        payload = json_object_object_get(inputs_obj, "payload");
        // print out the name attribute
        //printf("google json res payload ==> %s\n\n", json_object_get_string(payload));
  	}
  	
	if ((intent == NULL) || (payload == NULL)){
  	    printf("google json res intent is NULL or payload is NULL \n");
		return -1;
    }
	else{
  	    printf("google json res payload ==> %s\n", json_object_get_string(payload));
  	}
	
  	commands = json_object_object_get(payload, "commands");
	
	if (commands == NULL){
  	    printf("google json res commands is NULL \n");
		return -1;
    }
	else{
  	    printf("google json res commands ==> %s\n", json_object_get_string(commands));
  	}
  	
  	// commands is an array of objects
    arraylen = json_object_array_length(commands);
  	//printf("\n>>google json res 2nd arraylen= %d\n",arraylen);
  	for (i = 0; i < arraylen; i++) {
  	    commands_obj = json_object_array_get_idx(commands, i);
  		devices = json_object_object_get(commands_obj, "devices");
  		//printf("google json res devices ==> %s\n\n", json_object_get_string(devices));
  		execution = json_object_object_get(commands_obj, "execution");
  		//printf("google json res execution ==> %s\n\n", json_object_get_string(execution));
  	}
  	
	if(execution != NULL){
  	    arraylen = json_object_array_length(execution);
  	    //printf("\n>>google json res 3nd arraylen= %d\n",arraylen);
  	}else{
		printf("\n google json res execution is NULL \n");
		return -1;
	}
	
  	for (i = 0; i < arraylen; i++) {
  	    execution_obj = json_object_array_get_idx(execution, i);
  		command = json_object_object_get(execution_obj, "command");
  		printf("google json res command ==> %s\n\n", json_object_get_string(command));
  		params = json_object_object_get(execution_obj, "params");
  		//printf("google json res params ==> %s\n\n", json_object_get_string(params));
  	}
  	
	if ((command == NULL ) || (params == NULL)){
        printf("\ngoogle json res params command or params is NULL\n");
        return -1;
    }
	
	//get params example
  	//doorlockaction = json_object_object_get(params,"doorlockaction");
  	//printf("google json res doorlockaction ==> %s\n", json_object_get_string(doorlockaction));
	
	//gas_command_array[gas_command_count][]
	for(i = 0 ; i < gas_command_count; i++)
    {
		
		//printf("gas_command_array %d = %s\n",i,gas_command_array[i]);// for debug
		
		//com.pegatron.commands.FlashLight
		if(strcmp(json_object_get_string(command),gas_command_array[0]) == 0){

		    strcpy(out_param_cmd,json_object_get_string(command));
            action_param1 = json_object_object_get(params,"number");
  	        printf("\n json_c_parser() 0 google json res action_param1 ==> %s\n", json_object_get_string(action_param1));
            if(action_param1 != NULL)
	            strcpy(out_param1,json_object_get_string(action_param1));

	        action_param2 = json_object_object_get(params,"speed");
  	        printf("\n json_c_parser() 0 google json res action_param2 ==> %s\n", json_object_get_string(action_param2));
            if(action_param2 != NULL)
	            strcpy(out_param2,json_object_get_string(action_param2));

		}//com.pegatron.commands.DoorLookControl
		else if(strcmp(json_object_get_string(command),gas_command_array[1]) == 0){
            strcpy(out_param_cmd,json_object_get_string(command));
            action_param1 = json_object_object_get(params,"doorlockaction");
            printf("\n json_c_parser() 1 google json res action_param1 ==> %s\n", json_object_get_string(action_param1));
            if(action_param1 != NULL)
                strcpy(out_param1,json_object_get_string(action_param1));
 
		}//com.pegatron.commands.MusicControl
		else if(strcmp(json_object_get_string(command),gas_command_array[2]) == 0){
            strcpy(out_param_cmd,json_object_get_string(command));
            action_param1 = json_object_object_get(params,"status");
            printf("\n json_c_parser() 2 google json res action_param1 ==> %s\n", json_object_get_string(action_param1));
            if(action_param1 != NULL)
                strcpy(out_param1,json_object_get_string(action_param1));
 
		}//com.pegatron.commands.DisturbControl
		else if(strcmp(json_object_get_string(command),gas_command_array[3]) == 0){
            strcpy(out_param_cmd,json_object_get_string(command));
            action_param1 = json_object_object_get(params,"motion");
            printf("\n json_c_parser() 3 google json res action_param1 ==> %s\n", json_object_get_string(action_param1));
            if(action_param1 != NULL)
                strcpy(out_param1,json_object_get_string(action_param1));
		
        }//com.pegatron.commands.VolumeControl
		else if(strcmp(json_object_get_string(command),gas_command_array[4]) == 0){
		    strcpy(out_param_cmd,json_object_get_string(command));
            action_param1 = json_object_object_get(params,"switch");
            printf("\n json_c_parser() 4 google json res action_param1 ==> %s\n", json_object_get_string(action_param1));
            if(action_param1 != NULL)
                strcpy(out_param1,json_object_get_string(action_param1));
			
        }//com.pegatron.commands.VolumeValueControl
		else if(strcmp(json_object_get_string(command),gas_command_array[5]) == 0){
            strcpy(out_param_cmd,json_object_get_string(command));
            action_param1 = json_object_object_get(params,"number");
            printf("\n json_c_parser() 5 google json res action_param1 ==> %s\n", json_object_get_string(action_param1));
            if(action_param1 != NULL)
                strcpy(out_param1,json_object_get_string(action_param1));	
        }//com.pegatron.commands.PhoneCallControl
		else if(strcmp(json_object_get_string(command),gas_command_array[6]) == 0){
            strcpy(out_param_cmd,json_object_get_string(command));
            action_param1 = json_object_object_get(params,"callaction");
            printf("\n json_c_parser() 6 google json res action_param1 ==> %s\n", json_object_get_string(action_param1));
            if(action_param1 != NULL)
                strcpy(out_param1,json_object_get_string(action_param1));	
        }

		else{
            printf("son_c_parser() google json res UNKNOW COMMAND\n");
        }
	}
  	// Free
    json_object_put(root);
    return 0;
  }
#endif

int main(int argc, char** argv) {
  std::string credentials_file_path, api_endpoint, locale, html_out_command;
  #ifndef ENABLE_ALSA
    std::cerr << "ALSA audio input is not supported on this platform."
              << std::endl;
    return -1;
  #endif
  
#if 0
 /*    PEGA test json cpp lib */
    // create a JSON object
    json j_object = {{"one", 1}, {"two", 2}};
	//json j_object_one = "{\"requestId\":\"5b92a8e8-0000-26de-a86a-089e082f5c04\",\"inputs\":[{\"intent\":\"action.devices.EXECUTE\",\"payload\":{\"commands\":[{\"devices\":[{\"id\":\"default\"}],\"execution\":[{\"command\":\"com.pegatron.commands.DoorLookControl\",\"params\":{\"doorlockaction\":\"UNLOCK\"}}]}]}}]}";
	std::ifstream i("action.json");
	json j_object_two;
	i >> j_object_two;

	// call find
    auto it_two = j_object.find("two");
    auto it_three = j_object.find("three");
	auto action = j_object_two.find("doorlockaction");


    // print values
    std::cout << std::boolalpha;
    std::cout << "\"two\" was found: " << (it_two != j_object.end()) << '\n';
    std::cout << "value at key \"two\": " << *it_two << '\n';
    std::cout << "\"three\" was found: " << (it_three != j_object.end()) << '\n';
	
	std::cout << "\"doorlockaction\" was found: " << (action != j_object_two.end()) << '\n';
	std::cout << "value at key \"action\": " << *action << '\n';
#endif


  // Initialize gRPC and DNS resolvers
  // https://github.com/grpc/grpc/issues/11366#issuecomment-328595941
  grpc_init();
  if (!GetCommandLineFlags(argc, argv, &credentials_file_path,
                          &api_endpoint, &locale, &html_out_command)) {
    return -1;
  }

#if 0
  //printf("C++ main call json-c lib parse json file function\n");
  //json_c_parser(google_response_json_content);

#endif

   //init google assistant command class
   GoogleAssistant_voice_cmd gas_voice_cmd;
   
   
 while (true) {
    // Create an AssistRequest
    AssistRequest request;
    auto* assist_config = request.mutable_config();

    if (locale.empty()) {
      locale = kLanguageCode; // Default locale
    }
    if (verbose) {
      std::clog << "Using locale " << locale << std::endl;
    }
    // Set the DialogStateIn of the AssistRequest
    assist_config->mutable_dialog_state_in()->set_language_code(locale);

    // Set the DeviceConfig of the AssistRequest
    assist_config->mutable_device_config()->set_device_id(kDeviceInstanceId);
    assist_config->mutable_device_config()->set_device_model_id(kDeviceModelId);

    // Set parameters for audio output
    assist_config->mutable_audio_out_config()->set_encoding(
      AudioOutConfig::LINEAR16);
    assist_config->mutable_audio_out_config()->set_sample_rate_hertz(16000);

    // Set parameters for screen config
    assist_config->mutable_screen_out_config()->set_screen_mode(
      html_out_command.empty() ? ScreenOutConfig::SCREEN_MODE_UNSPECIFIED : ScreenOutConfig::PLAYING
    );

    std::unique_ptr<AudioInput> audio_input;
    // Set the AudioInConfig of the AssistRequest
    assist_config->mutable_audio_in_config()->set_encoding(
      AudioInConfig::LINEAR16);
    assist_config->mutable_audio_in_config()->set_sample_rate_hertz(16000);

    // Read credentials file.
    std::ifstream credentials_file(credentials_file_path);
    if (!credentials_file) {
      std::cerr << "Credentials file \"" << credentials_file_path
                << "\" does not exist." << std::endl;
      return -1;
    }
    std::stringstream credentials_buffer;
    credentials_buffer << credentials_file.rdbuf();
    std::string credentials = credentials_buffer.str();
    std::shared_ptr<CallCredentials> call_credentials;
    call_credentials = grpc::GoogleRefreshTokenCredentials(credentials);
    if (call_credentials.get() == nullptr) {
      std::cerr << "Credentials file \"" << credentials_file_path
                << "\" is invalid. Check step 5 in README for how to get valid "
                << "credentials." << std::endl;
      return -1;
    }

    // Begin a stream.
    auto channel = CreateChannel(api_endpoint);
    std::unique_ptr<EmbeddedAssistant::Stub> assistant(
        EmbeddedAssistant::NewStub(channel));

    grpc::ClientContext context;
    context.set_fail_fast(false);
    context.set_credentials(call_credentials);

    std::shared_ptr<ClientReaderWriter<AssistRequest, AssistResponse>>
        stream(std::move(assistant->Assist(&context)));
    // Write config in first stream.
    if (verbose) {
      std::clog << "assistant_sdk wrote first request: "
                << request.ShortDebugString() << std::endl;
    }
    stream->Write(request);

      audio_input.reset(new AudioInputALSA());

      audio_input->AddDataListener(
        [stream, &request](std::shared_ptr<std::vector<unsigned char>> data) {
          request.set_audio_in(&((*data)[0]), data->size());
          stream->Write(request);
      });
      audio_input->AddStopListener([stream]() {
        stream->WritesDone();
      });
      audio_input->Start();
   
    /*****************************************/
	//printf(">>> run assistant call deviceaction\n");
	//DeviceAction deviceaction;
	/*****************************************/  
	  
    AudioOutputALSA audio_output;
    audio_output.Start();

    // Read responses.
    if (verbose) {
      std::clog << ">>PEGA assistant_sdk waiting for response ... " << std::endl;
    }
	
    AssistResponse response;
	//printf("out PEGA response.event_type = %d\n",response.event_type());
	std::clog << ">>out PEGA response.event_type ="<< response.event_type() << std::endl;
    while (stream->Read(&response)) {  // Returns false when no more to read.
	       
          if (response.has_audio_out() ||
              response.event_type() == AssistResponse_EventType_END_OF_UTTERANCE) {
            // Synchronously stops audio input if there is one.
            if (audio_input != nullptr && audio_input->IsRunning()) {
              audio_input->Stop();
            }
          }
	      printf(">>PEGA response.event_type = %d\n",response.event_type()); //PEGA
          if (response.has_audio_out()) {
            // CUSTOMIZE: play back audio_out here.
            //std::clog << ">>PEGA assistant_sdk response has_audio_out... " << std::endl; //PEGA
            std::shared_ptr<std::vector<unsigned char>>
                data(new std::vector<unsigned char>);
            data->resize(response.audio_out().audio_data().length());
            memcpy(&((*data)[0]), response.audio_out().audio_data().c_str(),
                response.audio_out().audio_data().length());
            //std::clog << ">>PEGA assistant_sdk response audio_output.Send to alsa... " << std::endl; //PEGA
            audio_output.Send(data);
        
          }
	      //PEGA
          // CUSTOMIZE: render spoken request on screen
        
          for (int i = 0; i < response.speech_results_size(); i++) {
            google::assistant::embedded::v1alpha2::SpeechRecognitionResult result =
                response.speech_results(i);
            
            if (verbose) {
              std::clog << ">>> PEGA assistant_sdk SpeechRecognitionResult request: \n"
                        << result.transcript() << " ("
                        << std::to_string(result.stability())
                        << ")" << std::endl;
	    	  
            }
	    	//PEGA
            //std::clog << ">>>>>>>>>>>>>>>>>>>>>> gas_response_string = " << result.transcript().data() << std::endl;
            memset(gas_response_string,0,strlen(gas_response_string));
	    	strcpy(gas_response_string,result.transcript().data());
        
          }
            
          //PEGA
	      //if ( (not_get_okgoogle_flag == 1) && (response.event_type() == 1)){
          if ((response.event_type() == 1)){
              
              if(strncmp("OK Google",gas_response_string,9) == 0 )
              {
                  printf(">>>>>>>>>>>>>>>>>>>>> gas_response_string = %s\n",gas_response_string);
                  printf(">>>>>>>>>>>>>>>>>>>>> GET the \"OK Google\" \n");
				  not_get_okgoogle_flag = 0;
              }else{
                  printf(">>>>>>>>>>>>>>>>>>>>> gas_response_string = %s\n",gas_response_string);
                  printf(">>>>>>>>>>>>>>>>>>>>> Not GET the \"OK Google\" \n");
                  not_get_okgoogle_flag = 1;
                  audio_output.Stop();			  
              }
 	      }
        
          if (!html_out_command.empty() && response.screen_out().data().size() > 0) {
            std::string html_out_base64 = base64_encode(response.screen_out().data());
            system((html_out_command + " \"data:text/html;base64, " + html_out_base64 + "\"").c_str());
          } else if (html_out_command.empty()) {
            if (response.dialog_state_out().supplemental_display_text().size() > 0) {
              // CUSTOMIZE: render spoken response on screen
              std::clog << ">>> PEGA assistant_sdk response:" << std::endl;
              std::cout << response.dialog_state_out().supplemental_display_text()
                        << std::endl;
            }
          }
	    
	    //PEGA device action json message output.
	    if(not_get_okgoogle_flag == 0){
            if(response.has_device_action()){
	        	std::clog << "\n >>>>>> PEGA C++ code response device action device_action response data() =" << std::endl;
	        	std::clog << response.device_action().device_request_json().data() << "\n"<< std::endl;
	        	strcpy(g_res_json_msg,response.device_action().device_request_json().data());
            
                // json-C part
                #if JSON_C
                printf(">>>>>> PEGA C code g_res_json_msg = %s\r\n",g_res_json_msg);
	        	memset(out_param_1, 0, strlen(out_param_1));
	        	memset(out_param_2, 0, strlen(out_param_2));
	        	memset(out_param_cmd, 0, strlen(out_param_cmd));
	        	
	        	//C++ call json-c parser
                //json_c_ret = json_c_parser(g_res_json_msg,"speed","number",out_param_1,out_param_2);
                json_c_ret = json_c_parser(g_res_json_msg,gas_command_array,out_param_1,out_param_2);
	        	
	        	if( json_c_ret != 0 ){
                    printf("\n>> PEGA json-c parse error !! \n");
                }
	        	else{
					
#if GAS_DEBUG
                    // this is for checking google assistant response message debug
	        		printf(">> PEGA out command=%s\n",out_param_cmd);
                    if(strlen(out_param_1) > 0)
                        printf(">> PEGA out_param_1=%s\n",out_param_1);
                    if(strlen(out_param_2) > 0){
                        printf(">> PEGA out_param_2=%s\r\n",out_param_2);
                    }else{
                        printf("\r\n");
                    }
#endif					
                    /*
					when google assistant API response json file for customization command
					the function below will process related action.
					*/
					
					if(strcmp(out_param_cmd,MUSICCONTROL)){
                        std::clog << "voice command Music control >> "<< out_param_1 << "\n" << std::endl;
					    gas_voice_cmd.music_control(out_param_1); // PLAY STOP ETC..
                    }
                    else if(strcmp(out_param_cmd,DOORCONTROL)){
                        std::clog << "voice command Door lock control !\n"<< std::endl;
                    }
                    else if(strcmp(out_param_cmd,FLASHLIGHT)){
                        std::clog << "voice command FlashLight control !\n"<< std::endl;
                    }
                    else if(strcmp(out_param_cmd,DISTURB)){
                        std::clog << "voice command DisturbControl control !\n"<< std::endl;						
                    }					
                    else if(strcmp(out_param_cmd,VOLUME)){
                        std::clog << "voice command Volume control !\n"<< std::endl;
                    }					
                    else if(strcmp(out_param_cmd,VOLUMEVALUE)){
                        std::clog << "voice command Volume Value control !\n"<< std::endl;
                    }
                    else if(strcmp(out_param_cmd,PHONECALL)){
                        std::clog << "voice command Phone Call control !\n"<< std::endl;
                    }
					
					}
                #endif
            }
        }
    }



    audio_output.Stop();


    grpc::Status status = stream->Finish();
    if (!status.ok()) {
      // Report the RPC failure.
      std::cerr << "assistant_sdk failed, error: " <<
                status.error_message() << std::endl;
      return -1;
    }
  }

  return 0;
}
