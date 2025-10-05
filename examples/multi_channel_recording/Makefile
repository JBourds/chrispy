.PHONY: upload, monitor

default: upload monitor

upload :
	pio run -e aura --target upload --upload-port $(port) 

monitor : 
	pio device monitor -p $(port)


