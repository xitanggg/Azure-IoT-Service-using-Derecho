/**
 * run.csx is an IoT Hub trigger Azure function that first timpstamps T1
 * (T1 = Time this Azure Function starts running) and sends "HelloWorld"
 * via TCP to Azure Load Balancer that directs the traffic to one of the
 * VM in the VM Scale Set that hosts Derecho application. It receives
 * T2 (T2 = Time Derecho group receives Request) from the Derecho application
 * and timestamps T3 before it sends T1, T2, T3 back to the IoT device.
 *
 * To begin, replace your Azure Load Balancer IP at line 31
 * and Enter your IoTHub connection string at line 34
 *
 * Acknowledgement:
 * Send messages from the cloud to your device with IoT Hub (.NET) : https://docs.microsoft.com/en-us/azure/iot-hub/iot-hub-csharp-csharp-c2d
 * Socket TCP Client in C#: https://stackoverflow.com/questions/10182751/server-client-send-receive-simple-text
 * Print time format in C#: https://www.c-sharpcorner.com/blogs/date-and-time-format-in-c-sharp-programming1
 *
 * Xitang Zhao 2019-06-02
 **/

using System;
using Microsoft.Azure.Devices;
using System.Text;
using System.Net.Sockets;

public static void Run(string myIoTHubMessage, TraceWriter log)
{
    // Get current time T1 (T1 = Time Azure Function starts running)
    string T1 = DateTime.Now.ToString("T1:yyyy-MM-dd_HH:mm:ss.ffffff_UTCK");

    // Enter your Azure Load Balancer IP here
    const string LOAD_BALANCER_IP = "xxx.xxx.xxx.xxx";
    const int PORT_NUM = 80;
    // Enter your IoTHub connection string here to connect with IoT hub
    const string iot_hub_connection_string = "HostName=derecho-iot-hub.azure-devices.net;SharedAccessKeyName=iothubowner;SharedAccessKey=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

    // Create a TCPClient object at the IP and port
    TcpClient function_TCP_client = new TcpClient(LOAD_BALANCER_IP, PORT_NUM);
    NetworkStream function_network_stream = function_TCP_client.GetStream();

    // Encode "HelloWorld" and send to Azure Load Balancer
    byte[] hello_world_bytes = Encoding.ASCII.GetBytes("HelloWorld");
    function_network_stream.Write(hello_world_bytes, 0, hello_world_bytes.Length);

    // Receive T2 (T2 = Time Derecho group receives Request)
    byte[] T2_byte = new byte[function_TCP_client.ReceiveBufferSize];
    int byte_count = function_network_stream.Read(T2_byte, 0, function_TCP_client.ReceiveBufferSize);
    string T2 = Encoding.ASCII.GetString(T2_byte, 0, byte_count);
    function_TCP_client.Close();
    // log.Info("Received : " + Encoding.ASCII.GetString(T2, 0, byte_count));

    // Get current time T3 (T3 = Time Azure Function replys to laptop)
    string T3 = DateTime.Now.ToString("T3:yyyy-MM-dd_HH:mm:ss.ffffff_UTCK");

    // Make T1_T2_T3 into a message
    string T1_T2_T3 = T1 + T2 + T3;
    var T1_T2_T3_message = new Message(Encoding.ASCII.GetBytes(T1_T2_T3));

    // Create IoT Hub service client
    ServiceClient iot_hub_service_client = ServiceClient.CreateFromConnectionString(iot_hub_connection_string);

    // Send back message in the format of "T1:2019-06-03_23:47:43.430534_UTC-04:00T2:2019-06-03_23:47:43.445375_UTC-04:00T3:2019-06-03_23:47:43.446150_UTC-04:00"
    iot_hub_service_client.SendAsync("sensor-to-hub", T1_T2_T3_message);

    // Print for debug
    log.Info($"C# IoT Hub trigger function processed a message: {myIoTHubMessage}");
    log.Info($"C# IoT Hub trigger function send a message: {T1_T2_T3}");
}
