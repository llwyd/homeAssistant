../bin/homeAssistant: ../common/aux.o ../common/weather.o ../common/comms.o ../common/task.o ../common/sensor.o ../common/mqtt.o daemon.o
	gcc -g -lm ../common/aux.o ../common/weather.o ../common/comms.o ../common/task.o ../common/sensor.o ../common/mqtt.o daemon.o -o ../bin/homeAssistant
daemon.o: daemon.c 
	gcc -c -g daemon.c -o daemon.o
../common/sensor.o: ../common/sensor.c
	gcc -c -g ../common/sensor.c -o ../common/sensor.o
../common/comms.o: ../common/comms.c
	gcc -c -g ../common/comms.c -o ../common/comms.o
../common/mqtt.o: ../common/mqtt.c
	gcc -c -g ../common/mqtt.c -o ../common/mqtt.o
../common/task.o: ../common/task.c
	gcc -c -g ../common/task.c -o ../common/task.o
../common/weather.o: ../common/weather.c
	gcc -c -g ../common/weather.c -o ../common/weather.o
../common/aux.o: ../common/aux.c
	gcc -c -g ../common/aux.c -o ../common/aux.o
clean:
	rm ../bin/homeAssistant daemon.o ../common/aux.o ../common/weather.o ../common/task.o ../common/sensor.o ../common/comms.o ../common/mqtt.o
