#include "mbed.h"
#include "mbed_rpc.h"

#include "accelerometer_handler.h"
#include "config.h"
#include "magic_wand_model_data.h"

#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/micro/kernels/micro_ops.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/version.h"

#include "uLCD_4DGL.h"

#include "MQTTNetwork.h"
#include "MQTTmbed.h"
#include "MQTTClient.h"

#include "stm32l475e_iot01_accelero.h"

//DigitalOut myled1(LED1);
DigitalOut myled2(LED2);
DigitalOut myled3(LED3);
DigitalIn pusheenbutton(USER_BUTTON);
uLCD_4DGL uLCD(D1, D0, D2);
using namespace std::chrono;

BufferedSerial pc(USBTX, USBRX);
void gesture(Arguments *in, Reply *out);
void tilt(Arguments *in, Reply *out);
void stop_gesture(Arguments *in, Reply *out);
void stop_tilt(Arguments *in, Reply *out);
void receive_confirmed_degree(Arguments *in, Reply *out);

RPCFunction rpcgesture(&gesture, "GESTURE");
RPCFunction rpctilt(&tilt, "TILT");
RPCFunction rpcstopgesture(&stop_gesture, "STOPGESTURE");
RPCFunction rpcstoptilt(&stop_tilt, "STOPTILT");
RPCFunction rpcreceive_confirmed_degree(&receive_confirmed_degree, "RECEIVECONFIRMEDDEGREE");
Thread thread_gesture_thread;
Thread thread_gesture_confirm_thread;
Thread thread_tilt_thread;
Thread tilt_thread_anypoint;
Thread tilt_over_thread;


Thread mqtt_thread(osPriorityHigh);
EventQueue mqtt_queue;

// GLOBAL VARIABLES
WiFiInterface *wifi;
//InterruptIn btn2(USER_BUTTON);
InterruptIn button(USER_BUTTON);
EventQueue button_queue(32 * EVENTS_EVENT_SIZE);
Thread thread_eventqueue_button;

Ticker ticker_anypoint_tilt;

Timeout send_wifi;

//InterruptIn btn3(SW3);
volatile int message_num = 0;
volatile int arrivedcount = 0;
volatile bool closed = false;

const char* topic = "Mbed";

//int16_t pDataXYZ[3] = {0};
int16_t reference_XYZ[3] = {0};
int16_t now_a_XYZ[3] = {0};
double now_tilt_degree=0.0;
int idR[32] = {0};
int indexR = 0;

int x;
int gesture_number;
int confirmed_gesture_number=0;
int gesture_thread_return=0;
int confirmed_degree=0;
int stop_gesture_label=0;
int take_reference_a_success=0;
int mode=0;
//int tilt_over=0
//int* client_address;
//MQTT::Client<MQTTNetwork, Countdown> client_address;
int first_tilt_send=0;
int stop_tilt_label=0;

//-------------------------Wi-fi-----------------------------------------

void messageArrived(MQTT::MessageData& md) {
    MQTT::Message &message = md.message;
    char msg[300];
    sprintf(msg, "Message arrived: QoS%d, retained %d, dup %d, packetID %d\r\n", message.qos, message.retained, message.dup, message.id);
    printf(msg);
    ThisThread::sleep_for(1000ms);
    char payload[300];
    sprintf(payload, "Payload %.*s\r\n", message.payloadlen, (char*)message.payload);
    printf(payload);
    ++arrivedcount;
}

void publish_message(MQTT::Client<MQTTNetwork, Countdown>* client) {

    message_num++;
    MQTT::Message message;
    char buff[100];
    //sprintf(buff, "QoS0 Hello, Python! #%d", message_num);
    //sprintf(buff, "#%d  Degree:%d", message_num, confirmed_degree);
    if(mode==1){
      //sprintf(buff, "Mode: 1, Degree: %d, Tiltover: 0", confirmed_degree);
      sprintf(buff, "Mode: 1, Degree: %d", confirmed_degree);
    }else if(mode==2){
      if(first_tilt_send==0){
        sprintf(buff, "Mode: 2, %5d,%5d,%5d, %2.2f", now_a_XYZ[0],now_a_XYZ[1],now_a_XYZ[2],now_tilt_degree);
      }else{
        sprintf(buff, "Mode: 3");
      }
      //sprintf(buff, "Mode: 2, Degree: 00, Tiltover: 1");
      //sprintf(buff, "Mode: 2, %4d,%4d,%4d", now_a_XYZ[0],now_a_XYZ[1],now_a_XYZ[2]);
    }
    
    message.qos = MQTT::QOS0;
    message.retained = false;
    message.dup = false;
    message.payload = (void*) buff;
    message.payloadlen = strlen(buff) + 1;
    int rc = client->publish(topic, message);

    printf("rc:  %d\r\n", rc);
    printf("Puslish message: %s\r\n", buff);
}

void close_mqtt() {
    closed = true;
}


//-----------------------------------------------------------------------

//-------------------machine learning------------------------------------

constexpr int kTensorArenaSize = 60 * 1024;
uint8_t tensor_arena[kTensorArenaSize];

// Return the result of the last prediction
int PredictGesture(float* output) {
  // How many times the most recent gesture has been matched in a row
  static int continuous_count = 0;
  // The result of the last prediction
  static int last_predict = -1;

  // Find whichever output has a probability > 0.8 (they sum to 1)
  int this_predict = -1;
  for (int i = 0; i < label_num; i++) {
    if (output[i] > 0.8) this_predict = i;
  }

  // No gesture was detected above the threshold
  if (this_predict == -1) {
    continuous_count = 0;
    last_predict = label_num;
    return label_num;
  }

  if (last_predict == this_predict) {
    continuous_count += 1;
  } else {
    continuous_count = 0;
  }
  last_predict = this_predict;

  // If we haven't yet had enough consecutive matches for this gesture,
  // report a negative result
  if (continuous_count < config.consecutiveInferenceThresholds[this_predict]) {
    return label_num;
  }
  // Otherwise, we've seen a positive result, so clear all our variables
  // and report it
  continuous_count = 0;
  last_predict = -1;

  return this_predict;
}

//----------------------------------------------------------------------------

void gesture_confirm_thread(){
//int gesture_confirm_thread(){
    while(1){
        if(stop_gesture_label==1){
            return;
        }
        if(pusheenbutton==0){
          confirmed_gesture_number=gesture_number;
          //uLCD.locate(2,2);
          //uLCD.printf("Confirmed: %d",gesture_number);
          if(gesture_number==0){
            confirmed_degree=25;
            uLCD.locate(1,2);
            uLCD.printf("Confirmed: 25 degree.");
          }else if(gesture_number==1){
            confirmed_degree=50;
            uLCD.locate(1,2);
            uLCD.printf("Confirmed: 50 degree.");
          }else if(gesture_number==2){
            confirmed_degree=75;
            uLCD.locate(1,2);
            uLCD.printf("Confirmed: 75 degree.");
          }
          gesture_thread_return=1;

//------------------------Wi-fi--------------------------------------------
          wifi = WiFiInterface::get_default_instance();
    if (!wifi) {
            printf("ERROR: No WiFiInterface found.\r\n");
            //return -1;
            return;
    }


    printf("\nConnecting to %s...\r\n", MBED_CONF_APP_WIFI_SSID);
    int ret = wifi->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if (ret != 0) {
            printf("\nConnection error: %d\r\n", ret);
            //return -1;
            return;
    }

    NetworkInterface* net = wifi;
    MQTTNetwork mqttNetwork(net);
    static MQTT::Client<MQTTNetwork, Countdown> client(mqttNetwork);

    //TODO: revise host to your IP
    const char* host = "172.24.2.86";//172.20.10.2
    printf("Connecting to TCP network...\r\n");

    SocketAddress sockAddr;
    sockAddr.set_ip_address(host);
    sockAddr.set_port(1883);

    printf("address is %s/%d\r\n", (sockAddr.get_ip_address() ? sockAddr.get_ip_address() : "None"),  (sockAddr.get_port() ? sockAddr.get_port() : 0) ); //check setting

    int rc = mqttNetwork.connect(sockAddr);//(host, 1883);
    if (rc != 0) {
            printf("Connection error.");
            //return -1;
            return;
    }
    printf("Successfully connected!\r\n");

    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    data.clientID.cstring = "Mbed";

    if ((rc = client.connect(data)) != 0){
            printf("Fail to connect MQTT\r\n");
    }
    if (client.subscribe(topic, MQTT::QOS0, messageArrived) != 0){
            printf("Fail to subscribe\r\n");
    }

    //-------------------(added)-------------------------------------
    //static MQTT::Client<MQTTNetwork, Countdown>* client_address;
    //client_address=&client;
    //---------------------------------------------------------------

    mqtt_thread.start(callback(&mqtt_queue, &EventQueue::dispatch_forever));
    //pusheenbutton.rise(mqtt_queue.event(&publish_message, &client));
    //btn2.rise(mqtt_queue.event(&publish_message, &client));
    send_wifi.attach(mqtt_queue.event(&publish_message, &client),2s);

    int num = 0;
    while (num != 5) {
            client.yield(100);
            ++num;
    }

    while (1) {
            if (closed || stop_gesture_label==1) break;
            printf("11111\n");
            client.yield(500);
            ThisThread::sleep_for(500ms);
    }

    //printf("Ready to close MQTT Network......\n");

    //if ((rc = client.unsubscribe(topic)) != 0) {
    //        printf("Failed: rc from unsubscribe was %d\n", rc);
    //}
    //if ((rc = client.disconnect()) != 0) {
    //printf("Failed: rc from disconnect was %d\n", rc);
    //}

    //mqttNetwork.disconnect();
    //printf("Successfully closed!\n");

    //return 0;
    return;
}

//-------------------------------------------------------------------------
          //return;
      }
    }
    



//void gesture_thread(int argc, char* argv[]){
void gesture_thread(){
    // Whether we should clear the buffer next time we fetch data
  bool should_clear_buffer = false;
  bool got_data = false;

  // The gesture index of the prediction
  int gesture_index;

  // Set up logging.
  static tflite::MicroErrorReporter micro_error_reporter;
  tflite::ErrorReporter* error_reporter = &micro_error_reporter;

  // Map the model into a usable data structure. This doesn't involve any
  // copying or parsing, it's a very lightweight operation.
  const tflite::Model* model = tflite::GetModel(g_magic_wand_model_data);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    error_reporter->Report(
        "Model provided is schema version %d not equal "
        "to supported version %d.",
        model->version(), TFLITE_SCHEMA_VERSION);
    //return -1;
    return ;
  }

  // Pull in only the operation implementations we need.
  // This relies on a complete list of all the ops needed by this graph.
  // An easier approach is to just use the AllOpsResolver, but this will
  // incur some penalty in code space for op implementations that are not
  // needed by this graph.
  static tflite::MicroOpResolver<6> micro_op_resolver;
  micro_op_resolver.AddBuiltin(
      tflite::BuiltinOperator_DEPTHWISE_CONV_2D,
      tflite::ops::micro::Register_DEPTHWISE_CONV_2D());
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_MAX_POOL_2D,
                               tflite::ops::micro::Register_MAX_POOL_2D());
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_CONV_2D,
                               tflite::ops::micro::Register_CONV_2D());
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_FULLY_CONNECTED,
                               tflite::ops::micro::Register_FULLY_CONNECTED());
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_SOFTMAX,
                               tflite::ops::micro::Register_SOFTMAX());
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_RESHAPE,
                               tflite::ops::micro::Register_RESHAPE(), 1);

  // Build an interpreter to run the model with
  static tflite::MicroInterpreter static_interpreter(
      model, micro_op_resolver, tensor_arena, kTensorArenaSize, error_reporter);
  tflite::MicroInterpreter* interpreter = &static_interpreter;

  // Allocate memory from the tensor_arena for the model's tensors
  interpreter->AllocateTensors();

  // Obtain pointer to the model's input tensor
  TfLiteTensor* model_input = interpreter->input(0);
  if ((model_input->dims->size != 4) || (model_input->dims->data[0] != 1) ||
      (model_input->dims->data[1] != config.seq_length) ||
      (model_input->dims->data[2] != kChannelNumber) ||
      (model_input->type != kTfLiteFloat32)) {
    error_reporter->Report("Bad input tensor parameters in model");
    //return -1;
    return ;
  }

  int input_length = model_input->bytes / sizeof(float);

  TfLiteStatus setup_status = SetupAccelerometer(error_reporter);
  if (setup_status != kTfLiteOk) {
    error_reporter->Report("Set up failed\n");
    //return -1;
    return ;
  }

  error_reporter->Report("Set up successful...\n");

//--------------------(added)----------------------------------------------
thread_gesture_confirm_thread.start(gesture_confirm_thread);
//-------------------------------------------------------------------------

  while (true) {

//----------------------(added)--------------------------------------------
        if(gesture_thread_return==1){
            return;
        }
//-------------------------------------------------------------------------

    // Attempt to read new data from the accelerometer
    got_data = ReadAccelerometer(error_reporter, model_input->data.f,
                                 input_length, should_clear_buffer);

    // If there was no new data,
    // don't try to clear the buffer again and wait until next time
    if (!got_data) {
      should_clear_buffer = false;
      continue;
    }

    // Run inference, and report any error
    TfLiteStatus invoke_status = interpreter->Invoke();
    if (invoke_status != kTfLiteOk) {
      error_reporter->Report("Invoke failed on index: %d\n", begin_index);
      continue;
    }

    // Analyze the results to obtain a prediction
    gesture_index = PredictGesture(interpreter->output(0)->data.f);

    // Clear the buffer next time we read data
    should_clear_buffer = gesture_index < label_num;

    // Produce an output
    if (gesture_index < label_num) {
      error_reporter->Report(config.output_message[gesture_index]);
//--------------------(added)------------------------------------------------
      gesture_number=gesture_index;
      //uLCD.locate(1,1);
      //uLCD.printf("%d",gesture_number);

      if(gesture_number==0){
          uLCD.locate(1,1);
          uLCD.printf("Angle: 25 degree.");
      }else if(gesture_number==1){
          uLCD.locate(1,1);
          uLCD.printf("Angle: 50 degree.");
      }else if(gesture_number==2){
          uLCD.locate(1,1);
          uLCD.printf("Angle: 75 degree.");
      }

      //if(pusheenbutton==0){
      //    confirmed_gesture_number=gesture_number;
      //    uLCD.locate(2,2);
      //    uLCD.printf("Confirmed: %d",gesture_number);
      //    return;
      //}

      //if(gesture_thread_return==1){
      //    return;
      //}


//---------------------------------------------------------------------------
    }
  }
}

void gesture(Arguments *in, Reply *out){
    //x = in->getArg<double>();
    //printf("%d\r\n",x);

    mode=1;

    thread_gesture_thread.start(gesture_thread);
    myled2 = 1;
    ThisThread::sleep_for(500ms);
    myled2 = 0;
    ThisThread::sleep_for(500ms);
    myled2 = 1;
    ThisThread::sleep_for(500ms);
    myled2 = 0;
    ThisThread::sleep_for(500ms);
    myled2 = 1;
    ThisThread::sleep_for(500ms);
    myled2 = 0;


}

void stop_gesture(Arguments *in, Reply *out){
    stop_gesture_label=1;
    uLCD.cls();
    
    printf("Ready to reset......\n");

    //if ((rc = client.unsubscribe(topic)) != 0) {
    //        printf("Failed: rc from unsubscribe was %d\n", rc);
    //}
    //if ((rc = client.disconnect()) != 0) {
    //printf("Failed: rc from disconnect was %d\n", rc);
    //}

    //mqttNetwork.disconnect();
    //printf("Successfully closed!\n");

    //return;
    ThisThread::sleep_for(5s);
    NVIC_SystemReset();
    //ThisThread::sleep_for(500ms);
    //printf("Reset Degree:%d\n",confirmed_degree);
}

void receive_confirmed_degree(Arguments *in, Reply *out){
  confirmed_degree = in->getArg<double>();
  printf("Confirmed_degree back!!: %d\n", confirmed_degree);
}

void stop_tilt(Arguments *in, Reply *out){
  stop_tilt_label=1;
  uLCD.cls();

  printf("Ready to reset......\n");

  ThisThread::sleep_for(5s);
  NVIC_SystemReset();

}

void tilt_thread_any_point(){
    //ticker_anypoint_tilt.attach(&flip, 2s);
    /*NetworkInterface* net = wifi;
    MQTTNetwork mqttNetwork(net);
    MQTT::Client<MQTTNetwork, Countdown> client(mqttNetwork);
    //TODO: revise host to your IP
    const char* host = "172.24.2.86";//172.20.10.2
    printf("Connecting to TCP network...\r\n");

    SocketAddress sockAddr;
    sockAddr.set_ip_address(host);
    sockAddr.set_port(1883);

    printf("address is %s/%d\r\n", (sockAddr.get_ip_address() ? sockAddr.get_ip_address() : "None"),  (sockAddr.get_port() ? sockAddr.get_port() : 0) ); //check setting

    int rc = mqttNetwork.connect(sockAddr);//(host, 1883);
    if (rc != 0) {
            printf("Connection error.");
            //return -1;
            return;
    }
    printf("Successfully connected!\r\n");

    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    data.clientID.cstring = "Mbed";

    if ((rc = client.connect(data)) != 0){
            printf("Fail to connect MQTT\r\n");
    }
    if (client.subscribe(topic, MQTT::QOS0, messageArrived) != 0){
            printf("Fail to subscribe\r\n");
    }*/


    //-------------------------(Wi-fi)------------------------------------

    wifi = WiFiInterface::get_default_instance();
    if (!wifi) {
            printf("ERROR: No WiFiInterface found.\r\n");
            //return -1;
            return;
    }


    printf("\nConnecting to %s...\r\n", MBED_CONF_APP_WIFI_SSID);
    int ret = wifi->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if (ret != 0) {
            printf("\nConnection error: %d\r\n", ret);
            //return -1;
            return;
    }

    NetworkInterface* net = wifi;
    MQTTNetwork mqttNetwork(net);
    static MQTT::Client<MQTTNetwork, Countdown> client(mqttNetwork);

    //TODO: revise host to your IP
    const char* host = "172.24.2.86";//172.20.10.2
    printf("Connecting to TCP network...\r\n");

    SocketAddress sockAddr;
    sockAddr.set_ip_address(host);
    sockAddr.set_port(1883);

    printf("address is %s/%d\r\n", (sockAddr.get_ip_address() ? sockAddr.get_ip_address() : "None"),  (sockAddr.get_port() ? sockAddr.get_port() : 0) ); //check setting

    int rc = mqttNetwork.connect(sockAddr);//(host, 1883);
    if (rc != 0) {
            printf("Connection error.");
            //return -1;
            return;
    }
    printf("Successfully connected!\r\n");

    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    data.clientID.cstring = "Mbed";

    if ((rc = client.connect(data)) != 0){
            printf("Fail to connect MQTT\r\n");
    }
    if (client.subscribe(topic, MQTT::QOS0, messageArrived) != 0){
            printf("Fail to subscribe\r\n");
    }

    
    mqtt_thread.start(callback(&mqtt_queue, &EventQueue::dispatch_forever));
    ////pusheenbutton.rise(mqtt_queue.event(&publish_message, &client));
    ////btn2.rise(mqtt_queue.event(&publish_message, &client));
    //send_wifi.attach(mqtt_queue.event(&publish_message, &client),2s);

    

    //--------------------------------------------------------------------

    //ThisThread::sleep_for(2s);
    ThisThread::sleep_for(1s);
    first_tilt_send=1;
    mqtt_queue.call(&publish_message, &client);
    ThisThread::sleep_for(1s);
    first_tilt_send=0;

    while(1){
        if(stop_tilt_label==1){
            return;
        }
        BSP_ACCELERO_AccGetXYZ(now_a_XYZ);
        //printf("%d, %d, %d\n", now_a_XYZ[0], now_a_XYZ[1], now_a_XYZ[2]);
        now_tilt_degree=(acos((double)now_a_XYZ[2]/(double)reference_XYZ[2]))/3.14159*180;
        printf("%d, %d, %d, %2.2f\n", now_a_XYZ[0], now_a_XYZ[1], now_a_XYZ[2], now_tilt_degree);
        uLCD.cls();
        uLCD.locate(1,1);
        uLCD.printf("x: %d",now_a_XYZ[0]);
        uLCD.locate(1,3);
        uLCD.printf("y: %d",now_a_XYZ[1]);
        uLCD.locate(1,5);
        uLCD.printf("z: %d",now_a_XYZ[2]);
        uLCD.locate(1,7);
        uLCD.printf("Angle: %f",now_tilt_degree);
        //ThisThread::sleep_for(1s);
        if(confirmed_degree == 25){
          //if(now_a_XYZ[2]<889){
          //if(now_tilt_degree > ((25/180)*3.14159) ){
          if(now_tilt_degree > 25 ){
            uLCD.locate(1,9);
            uLCD.printf("Exceed %d degree!",confirmed_degree);
            printf("Exceed %d degree!\n",confirmed_degree);
            //tilt_over=1;
            //tilt_over_thread.start(publish_message);
            //tilt_over_thread.start(mqtt_queue.event(&publish_message, &client));
            mqtt_queue.call(&publish_message, &client);
            //tilt_over_thread.start(mqtt_queue.event(&publish_message, client_address));
          }
        }else if(confirmed_degree == 50){
          //if(now_a_XYZ[2]<630){
          //if(now_tilt_degree > ((50/180)*3.14159) ){
          if(now_tilt_degree > 50 ){
            uLCD.locate(1,9);
            uLCD.printf("Exceed %d degree!",confirmed_degree);
            printf("Exceed %d degree!\n",confirmed_degree);
            //tilt_over_thread.start(publish_message);
            //tilt_over_thread.start(mqtt_queue.event(&publish_message, &client));
            mqtt_queue.call(&publish_message, &client);
            //tilt_over_thread.start(mqtt_queue.event(&publish_message, client_address));
          }
        }else if(confirmed_degree == 75){
          //if(now_a_XYZ[2]<254){
          //if(now_tilt_degree > ((75/180)*3.14159) ){
          if(now_tilt_degree > 75 ){
            uLCD.locate(1,9);
            uLCD.printf("Exceed %d degree!",confirmed_degree);
            printf("Exceed %d degree!\n",confirmed_degree);
            //tilt_over_thread.start(publish_message);
            //tilt_over_thread.start(mqtt_queue.event(&publish_message, &client));
            mqtt_queue.call(&publish_message, &client);
            //tilt_over_thread.start(mqtt_queue.event(&publish_message, client_address));
          }
        }
        ThisThread::sleep_for(1s);

//-------------------------------------------------------------------------
    //    int num = 0;
    //while (num != 5) {
    //        client.yield(100);
    //        ++num;
    //}

    //while (1) {
    //        if (closed || stop_gesture_label==1) break;
    //        printf("11111\n");
    //        client.yield(500);
    //        ThisThread::sleep_for(500ms);
    //}

    
    //return;
//---------------------------------------------------------------------------

    }
}

void take_reference_a(){
    BSP_ACCELERO_AccGetXYZ(reference_XYZ);
    printf("%d, %d, %d\n", reference_XYZ[0], reference_XYZ[1], reference_XYZ[2]);
    myled3 = 0;
    printf("Take_reference_a_success!!\n");
    tilt_thread_anypoint.start(tilt_thread_any_point);
    take_reference_a_success=1;
}

void take_reference_a_midway(){
    button_queue.call(take_reference_a);
}


void tilt_thread(){
    myled3 = 1;
    //button.rise(&take_reference_a);
    printf("Start accelerometer init\n");
    BSP_ACCELERO_Init();
    thread_eventqueue_button.start(callback(&button_queue, &EventQueue::dispatch_forever));
    button.rise(take_reference_a_midway);
    while(1){
        if(take_reference_a_success==1){
            return;
        }
    }
}

void tilt(Arguments *in, Reply *out){
    //x = in->getArg<double>();
    //printf("%d\r\n",x);

    //uLCD.locate(1,2);
    //uLCD.printf("weeee.");

    mode=2;

    thread_tilt_thread.start(tilt_thread);
    myled2 = 1;
    ThisThread::sleep_for(500ms);
    myled2 = 0;
    ThisThread::sleep_for(500ms);
    myled2 = 1;
    ThisThread::sleep_for(500ms);
    myled2 = 0;
    ThisThread::sleep_for(500ms);
    myled2 = 1;
    ThisThread::sleep_for(500ms);
    myled2 = 0;
}



int main() {

    char buf[256], outbuf[256];

    FILE *devin = fdopen(&pc, "r");
    FILE *devout = fdopen(&pc, "w");

    printf("RPC mode selection\r\n");
    //printf("%f\r\n",acos((double)5));
    myled3=0;
    myled2=0;

    while(1) {
        memset(buf, 0, 256);
        for (int i = 0; ; i++) {
            char recv = fgetc(devin);
            if (recv == '\n') {
                printf("\r\n");
                break;
            }
            buf[i] = fputc(recv, devout);
        }
        RPC::call(buf, outbuf);
        printf("%s\r\n", outbuf);
    }
}