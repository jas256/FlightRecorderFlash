#include <Arduino.h>
#include <SPIMemory.h>
#include <FlightRecorder.h>

// Memory Info
MemoryMap1M map_1mb;
const uint32_t memory_size = MB(1);

FlightRecorder recorder(memory_size, 5, map_1mb);

void setup()
{
    Serial.begin(115200);
    recorder.begin();
    Serial.println("Recorder Setup");
    uint8_t flight_no = recorder.beginFlight(true);
    Serial.printf("Flight Number = %d \n", flight_no);
    uint32_t start_time = micros();
    int i = 0;
    FlightInfoFrame dataframe1;
    dataframe1.ground_pressure = 101325.0;
    dataframe1.ground_temperature = 15.0;
    dataframe1.absolute_pressure = 102325.0;
    dataframe1.timestamp = 0;
    dataframe1.temperature = 20.3;
    recorder.saveDataframe(dataframe1);
    i++;
    for (; i < 180; i++)
    {
        FlightInfoFrame dataframe;
        dataframe.ground_pressure = 101325.0;
        dataframe.ground_temperature = 15.0;
        dataframe.absolute_pressure = 102325.0 - (2000 * sin(DEG_TO_RAD * (i)));
        dataframe.timestamp = (i) * 500;
        dataframe.temperature = 20.3;
        recorder.saveDataframe(dataframe);
    }
    uint32_t end_time = micros();
    recorder.endFlight();
    Serial.printf("Completed Writing in %lu \n\nData: \n\n", end_time-start_time);

    int flight_no = 0;

    Serial.printf("Printing Data for Flight %d\n\n", flight_no);
    Serial.printf("timestamp,abs pressure,\n");
    for (int i = 0; i < 180; i++)
    {
        FlightInfoFrame readframe;
        recorder.loadDataframe(flight_no,i,&readframe);
        Serial.printf("%lu,%4.2f\n", readframe.timestamp, readframe.absolute_pressure);
    }
    RecorderMetadata data_meta = recorder.getMetadata();
    Serial.printf("Apogee is %f , Flight Time is %lu and records is %lu",data_meta.max_altitudes[flight_no], data_meta.flight_times[flight_no],data_meta.flight_rows_used[flight_no]);
}

void loop()
{
}