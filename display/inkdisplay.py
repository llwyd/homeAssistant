import argparse
import paho.mqtt.client as mqtt
import time
from inky.auto import auto
from PIL import Image, ImageFont, ImageDraw
from font_fredoka_one import FredokaOne
from font_hanken_grotesk import HankenGroteskBold

rcv_lounge_temp = False
rcv_lounge_hum = False
rcv_bedroom_temp = False
rcv_bedroom_hum = False

lounge_temp = ""
lounge_hum = ""
bedroom_temp = ""
bedroom_hum = ""

def on_message(client, userdata, message):
    full_topic = message.topic
    topic = full_topic.replace('/',' ').split()[-1]
    loc = full_topic.replace('/',' ').split()[-2]
    data = message.payload.decode()
    
    global rcv_lounge_temp
    global rcv_lounge_hum
    global rcv_bedroom_temp
    global rcv_bedroom_hum
    global lounge_temp
    global lounge_hum
    global bedroom_temp
    global bedroom_hum

    if loc == 'lounge_area' and topic =='temp_live' and not rcv_lounge_temp:
        lounge_temp = data
        rcv_lounge_temp = True
    elif loc == 'lounge_area' and topic == 'hum_live' and not rcv_lounge_hum:
        lounge_hum = data
        rcv_lounge_hum = True
    elif loc == 'bedroom_area' and topic == 'temp_live' and not rcv_bedroom_temp:
        bedroom_temp = data
        rcv_bedroom_temp = True
    elif loc == 'bedroom_area' and topic == 'hum_live' and not rcv_bedroom_hum:
        bedroom_hum = data
        rcv_bedroom_hum = True

    if( rcv_lounge_temp and rcv_bedroom_temp and rcv_lounge_hum and rcv_bedroom_hum ):
        print("  Lounge: ","{0:.1f}".format(float(lounge_temp)), "C ", "{0:.1f}".format(float(lounge_hum)),"%")
        print(" Bedroom: ","{0:.1f}".format(float(bedroom_temp)), "C ", "{0:.1f}".format(float(bedroom_hum)),"%")

        client.loop_stop()

# Initialise display
display = auto()
display.set_border(display.BLACK)
print("Initialising Display...")
print("Colour: " + str(display.colour) )
print("   Res: " + str(display.resolution) )
print("")

# Init MQTT
client = mqtt.Client()
client.on_message = on_message

# Get IP of broker via argparse
parser = argparse.ArgumentParser()
parser.add_argument("ip", help = "IP address of MQTT broker")
args = parser.parse_args()

broker_ip = str(args.ip)

client.connect(broker_ip, 1883, 60)
client.subscribe("home/#")

print("Collecting Data via MQTT...")
client.loop_start()
time.sleep(5)

# Output to display
# Initialise canvas
img = Image.new( "P", (display.WIDTH, display.HEIGHT) )
draw = ImageDraw.Draw(img)

# Init font
font = ImageFont.truetype(HankenGroteskBold, 22)

def AddText( d, xy, s, data, f ):
    label = s
    data_str = "{0:.1f}".format(float(data))+"C"
    full_string = label + data_str
    d.text( xy, full_string, display.BLACK, f )

#draw.text( (0, 0),  "    Lounge:", display.YELLOW, font)

AddText( draw, (0, 44), "    Lounge: ", lounge_temp, font )
AddText( draw, (0, 66), "Bedroom: ", bedroom_temp, font )

print("Updating display...")
display.set_image(img)
display.show()

print("FIN")



