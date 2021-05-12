# -*- coding: utf-8 -*-
"""
Created on Wed Apr  7 10:54:21 2021

@author: haava
"""
#Matematikk og tid:
import numpy as np 
from datetime import datetime , timedelta
from time import sleep, time

# Pandas:
import pandas as pd

# MQTT:
import paho.mqtt.client as mqtt
import paho.mqtt.publish as publish

#CoT:
import requests
import json


rom_i_kollektiv=["Stue","Kjøkken","Bad", "Klesvask"]            #Liste for rom i kollektivet
personer_i_kollektiv=["Lars","Haavard","Karl","Tormod"]         #Liste for personer i kollektivet

cols=["Rom","Personer","Gjester","booking_tid"]                 #Kolonnenavn i bookingfilen


df=pd.read_csv("booking.csv")           #Henter bookingfilen som en dataframe
df_1=pd.DataFrame(columns=cols)         #Setter kolonnene til kolonnelisten


#CoT signalinformasjon:
Tokens = ["", 
          "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1MTkwIn0.-xszm_AFkXDZTnevdjFy9Ivvozdp02EwBSulSWwiOHg",
          "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1OTIyIn0.-QervryV2oRrnpeFFmbx7RGGSLwKmIWhKN3X1mAXCy0",
          "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1OTY2In0.jpA9BqhHik3DZACJT-aMcrSunlvQeyid9snyh6WxPTA"]

lysKeys = ["23930", "11225", "8827", "30168"]
tempKeys = ["3530", "5429", "5490", "27206"]


# MQTT broker, port og ID:
broker = "192.168.137.85" 
port = 1883
clientID = "RPIclient"

# MQTT TOPICS:
subTopic = "booking/rpi/sub"
pubTopic = "booking/rpi/pub"
statustopic = "booking/status"

# Globale verdier som brukes ved tilbakemelding om booking til konsollen. Disse må være globale
bookeperson, bookerom, bookegjester, bookStartTid, bookSluttTid= "", "", "", "", ""


def round_minutes(dt, resolutionInMinutes):
    # Runder av til nærmeste minutt
    dtTrunc = dt.replace(second=0, microsecond=0)

    # Finner ut hvor lenge siden forrige hele femminutt
    excessMinutes = (dtTrunc.hour*60 + dtTrunc.minute) % resolutionInMinutes

    # Subtraherer resten fra tiden nå for å runde ned til nærmeste femmunitt
    return dtTrunc + timedelta(minutes=-excessMinutes)   

    #Funksjon som bergener antall folk i stua til en hver tid
def antall_gjester(rom,gjester_i_stue):
    antall=0
    for i in gjester_i_stue:
        antall+=df["Gjester"].iloc[i]
    return antall

    # Funksjon som tar seg av hele bookingen
def booking(starttid,sluttid,rom,person,gjester):
    antall = 0
    global df, df_1
    
    # Leser csv-filene og oppdaterer info om bookinger.
    df=pd.read_csv("booking.csv")
    df_1=pd.DataFrame(columns=cols)
    
    # Henter nåværende tid og runder ned til nærmeste femminutt
    now=round_minutes(datetime.now(), 5)
    
    # multipliserer gitt tidsverdier med 5, siden konsollen sender antall femminuttsintervaller
    booking_start = now + timedelta(minutes=starttid*5)
    booking_slutt = now + timedelta(minutes=sluttid*5)
    
    # Henter tidsintervall
    intervall=sluttid-starttid
    
    global bookeperson, bookerom, bookegjester, bookStartTid, bookSluttTid

    #Oppdaterer tilbakemeldingsverdiene til 
    bookeperson = personer_i_kollektiv[person]
    bookerom = rom_i_kollektiv[rom]
    bookegjester = str(gjester)
    bookStartTid = str(booking_start)
    bookSluttTid = str(booking_slutt)
    
    for i in range(0,intervall+1):
        
        # Legger til hvert femminuttsintervall til dataframen
        booketid = str((booking_start+timedelta(minutes=i*5)))
        df_1.loc[i] = [rom_i_kollektiv[rom],personer_i_kollektiv[person],gjester,booketid]
        
        # Lager en liste av alle tidspunker med gjester
        liste_av_gjester=df[(df['Rom']  == "Stue") & (df['booking_tid'] == booketid)].index.tolist()
        
        # Sjekker om det er gjester. Finner ut hvor mange gjester det er på stua gitt et tidspunkt
        if len(liste_av_gjester)!=0 and rom_i_kollektiv[rom]=="Stue":
            antall=antall_gjester(rom_i_kollektiv[rom], liste_av_gjester)
        
        # Sjekker om kjøkkenet eller badet allerede er booket
        if rom!=0 and len(df[df.Rom==rom_i_kollektiv[rom]][df.booking_tid==booketid])>0:
            publish_mqtt("Room busy")
            return False
        
        # Sjekker om samme person har booket et rom fra før
        elif len(df[df.Personer==personer_i_kollektiv[person]][df.booking_tid==booketid])!=0:
            publish_mqtt("2x booked")
            return False
        
        # Sjekker om stua er fullbooka
        elif len(df[df.Rom=="Stue"][df.booking_tid==booketid]) + gjester + antall >2 and rom==0:
            publish_mqtt("Overcrowded")
            return False
        
    # Legger df_1 til df
    return pd.concat([df,df_1])
            
def Write_to_df(DF):
    global bookStartTid, bookSluttTid, bookeperson, bookerom, bookegjester
    
    # Sjekker om bookingen returnerer en bool
    if isinstance(DF,bool)==True:
        status_update("Booking Failed")
        status_update(bookeperson+" tried to book "+bookerom)
        status_update("from "+bookStartTid+" to "+bookSluttTid)
        status_update("With "+bookegjester+" guests.")
    
        publish_mqtt("False")
        return False
    
    # Om bookingen ikke ga en bool, skrives dataframen til en csv-fil
    DF.to_csv(r"booking.csv",index=False,header=True)
    
    #Kringkaster info om godkjent booking
    status_update("Booking successful!")
    status_update(bookeperson+" booked "+bookerom)
    status_update("from "+bookStartTid+" to "+bookSluttTid)
    status_update("With "+bookegjester+" guests.")
    
    #Melder godkjent tilbake til konsollen
    publish_mqtt("True")


    #Funksjon som kjøres ved tilkoblig til mqtt:
def on_connect(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))    #Melder om tilkobling lykkes
    client.subscribe(subTopic)                      #Abonnerer til riktig signal



bookData = []       #Liste for lagring av data før databehandling

#Funksjon som kjøres ved mottak av ny melding:
def on_message(client, userdata, msg):
    global bookData             #skaffer tilgang på bookdatalisten utenfor funksjonen
    
    message = str(msg.payload.decode("utf-8"))      #Omgjør meldingen fra payload til string
    print(msg.topic+" "+message)                    #Printer meldingen
    
    if(message == "-1"):              #Sjekker etter nøkkelverdi -1, og klargjør så programmet for ny melding
        bookData.clear()              #Renser bookdata fra en eventuell tidligere beskjed
        print("")
        print("START OF MESSAGE")
    else:
        bookData.append(int(message))           #Legger beskjeden til listen dersom det ikke er -1
    
    
    if(len(bookData) == 7):                             #venter til listen har en lengde på 7 som er avtalt meldinglengde
    
        if(bookData[0] == 1):                           #1 er koden for å åpne døren. 
            status_update("Opening Door")               
            client.publish("project/door", "Open")      #Sender beskjed om å åpne døren til døråpneren.
            
        elif(bookData[0] == 2):                         #2 er koden for å endre temperatur i et rom
        
            print("Relaying temp to CoT")
            
                                                        #Sender ny temperaturveri til CoT
            if(bookData[2] < 2):
                publish_to_cot(int(bookData[2]), tempKeys[int(bookData[1])], Tokens[2])     
            else:
                publish_to_cot(int(bookData[2]), tempKeys[int(bookData[1])], Tokens[1])
             
        elif(bookData[0] == 3):                         #3 er koden for å endre lysverdier i CoT
        
            print("Relaying light value to CoT")
            publish_to_cot(int(bookData[2]), lysKeys[int(bookData[1])], Tokens[3])  #Sender ny verdi til CoT
            
        elif(bookData[0] == 0):                                                     #0 er koden for å booke rom         
            status_update("Attempt booking")                                        
            Write_to_df(booking(bookData[3], bookData[4],                           #Forsøker booking med dataen mottatt fra mqtt
                                bookData[2], bookData[1], bookData[5]))

        bookData.clear()        #Tømmer listen for neste melding
        print("END OF MESSAGE")


#Funksjon sender melding via MQTT
def publish_mqtt(_message):
    client.publish(pubTopic, _message)
    print("publish_mqtt run")

#Funksjon som sender statusmelding direkte til MQTT-serveren
def status_update(status):
    client.publish(statustopic, status)
    print("Status updated to: "+status)

#Funksjon som kalles hver gang scriptet sender en melding til MQTT. For debugbruk.
def on_publish(mosq, obj, mid):
    print("mid: " + str(mid))
    print("on_publish run")


#Binder egendefinerte funksjoner til funksjoner i paho-bibilioteket.
client = mqtt.Client(clientID)
client.on_connect = on_connect
client.on_message = on_message
client.connect(broker, port, 60)

#Melder i fra om at RPi-ern er online. 
status_update("Rpi online o7")

#Starter client.loop. Denne ligger i bakgrunnen og sjekker kontinuelig om nye meldinger i abonnerte kanaler. 
client.loop_start()


#Funksjon som tar seg av CoT-opplasting
def publish_to_cot(cotValue, cotKey, cotToken):
    cotInfo={'Key':'0','Value':0,'Token':'0'} 

    cotInfo['Key']=cotKey 
    cotInfo['Value']=cotValue
    cotInfo['Token']=cotToken
 
    response=requests.put('https://circusofthings.com/WriteValue',
                          data=json.dumps(cotInfo),headers={'Content-Type':'application/json'})

while True:
    
    sleep(300) 
    
    
