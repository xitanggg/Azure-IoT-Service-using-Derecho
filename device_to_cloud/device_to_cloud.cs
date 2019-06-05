/**
 * device_to_cloud.cs uses your laptop as an IoT device that sends T0
 * (T0 = Time laptop about to send message) to IoT Hub every 3s. Then
 * it prints T0, T1, T2, T3, T4 when it hears back.
 *
 * To begin, enter your device connection string at line 34.
 * Run "dotnet restore" followed by "dotnet run" on your terminal.
 *
 * My finding: The first IoT Hub connection takes ~1-2s but the rest is super fast
 * T0-T4 round trip is ~100ms on average, T1-T3 takes ~5ms on average
 * the bottleneck is potentially on the IoT Hub binding and function triggering.
 *
 * Acknowledgement:
 * Quickstart: Send telemetry from a device to an IoT hub and read it with a back-end application (C#) :
 *             https://docs.microsoft.com/en-us/azure/iot-hub/quickstart-send-telemetry-dotnet
 * Print time format in C#: https://www.c-sharpcorner.com/blogs/date-and-time-format-in-c-sharp-programming1
 * This application uses the Azure IoT Hub device SDK for .NET
 * For samples see: https://github.com/Azure/azure-iot-sdk-csharp/tree/master/iothub/device/samples
 *
 * Xitang Zhao 2019-06-02
 **/

using System;
using Microsoft.Azure.Devices.Client;
using System.Text;
using System.Net.Sockets;
using System.Threading;

namespace device_to_cloud_namespace
{
    class device_to_cloud
    {
        // Enter your device connection string here to connect with IoT hub.
        private readonly static string device_connection_string = "HostName=derecho-iot-hub.azure-devices.net;DeviceId=my-device-1;SharedAccessKey=xxxxxxxxxxxxxxxxxxxx/xxxxxxxxxxx/xxxxxxxxxxx";

        private static DeviceClient device_client;

        private static string T0, T1, T2, T3, T4, T1_T2_T3;

        private static bool stop = true;

        // Async method to send data - device-to-cloud (D2C)
        private static async void Send_D2C_Message()
        {
            // Get current time T0 (T0 = Time laptop about to send message)
            T0 = DateTime.Now.ToString("T0:yyyy-MM-dd_HH:mm:ss.ffffff");
            //T0 = DateTime.Now.ToString("T0:yyyy-MM-dd_HH:mm:ss.ffffff_UTCK");

            // Make into message
            var T0_message = new Message(Encoding.ASCII.GetBytes(T0));

            // Send time message
            await device_client.SendEventAsync(T0_message);
        }

        // Async method to receive data - cloud-to-device (C2D)
        private static async void Receive_C2D_Message()
        {
            while (true)
            {
                // Wait to receive message
                Message receivedMessage = await device_client.ReceiveAsync();

                // Redo while loop if not receive
                if (receivedMessage == null) continue;

                // If receive, get current time T4 (T4 = Time laptop hears back from function)
                T4 = DateTime.Now.ToString("T4:yyyy-MM-dd_HH:mm:ss.ffffff");

                // Receive message is T1_T2_T3
                T1_T2_T3 = Encoding.ASCII.GetString(receivedMessage.GetBytes());

                // Completes message and delete it from IoT Hub.
                await device_client.CompleteAsync(receivedMessage);

                // Extract T1-T3
                string[] T1_T2_T3_split = T1_T2_T3.Split("_UTC-04:00");
                T1 = T1_T2_T3_split[0];
                T2 = T1_T2_T3_split[1];
                T3 = T1_T2_T3_split[2];

                // Print T0-T4
                Console.WriteLine("T0-T4 all received!");
                Console.WriteLine("T0 = Time laptop about to send message    :  {0}", T0);
                Console.WriteLine("T1 = Time Azure Function starts running   :  {0}", T1);
                Console.WriteLine("T2 = Time Derecho group receives Request  :  {0}", T2);
                Console.WriteLine("T3 = Time Azure Function replys to laptop : {0}", T3);
                Console.WriteLine("T4 = Time laptop hears back from function :  {0}", T4);

                // Break while loop
                stop = false;
                break;
            }
        }

        private static void Main(string[] args)
        {
            Console.WriteLine("Connect device to the IoT hub.\n");
            // Create and connect device to the IoT hub using the MQTT protocol
            device_client = DeviceClient.CreateFromConnectionString(device_connection_string, TransportType.Mqtt);

            // Send T0 and get T0-T4 every 3 seconds
            // Stop is used to prevent sending the next message before the previous is done
            while (true){
                Console.WriteLine("Send T0 to the IoT hub.");
                Send_D2C_Message();
                Receive_C2D_Message();
                while (stop){
                    Thread.Sleep(3000);
                }
                stop = true;
            }
        }
    }
}
