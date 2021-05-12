# -*- coding: utf-8 -*-
"""
Created on Tue Apr 20 12:01:33 2021

@author: haava
"""
from time import time,sleep
from metno_locationforecast import Place, Forecast
from datetime import datetime
import json 
import requests
import pandas as pd

df=pd.read_csv("df_merged.csv")
df=df.drop("Position",axis=1)
index_lavest_pris=df["Price_amount"].idxmin()
vaskemaskin_start=datetime.strptime(df["PriceTime"].iloc[index_lavest_pris],"%Y-%m-%d %H:%M:%S")

# Henter værdataen fra yr
def vær_data():
    
    USER_AGENT = "metno_locationforecast/1.0 karlebr@stud.ntnu.no" 
    
    Trheim = Place("Trheim", 63.41923433502319, 10.402312395989917, 50) 
    
    Trheim_forcast = Forecast(Trheim, USER_AGENT)
    
    Trheim_forcast.update()
    
    interval = Trheim_forcast.data.intervals[1]
    
    print (interval)
    
    skyer = interval.variables["cloud_area_fraction"]
    skyer_prosent = skyer.value
    
    air_temperature = interval.variables["air_temperature"]
    temp = air_temperature.value
    return temp,skyer_prosent
    
print(vær_data())

# Sender data til COT
def send_data(KEY_1,TOKEN_1,VALUE_1):
    
    # Lager dictonary
 
    data_1={'Key':'0','Value':0,'Token':'0'} 
 
 
    # definer key, value og token
 
    data_1['Key']=KEY_1 
    data_1['Value']=VALUE_1
    data_1['Token']=TOKEN_1
 
    # Sender request til CoT
 
    response=requests.put('https://circusofthings.com/WriteValue',
                          data=json.dumps(data_1),headers={'Content-Type':'application/json'})
      
while True:
    
    # Sjekker hva tid det er nå.
    date=datetime.now()
    if 7<=date.hour<23:
        
        # Hvis klokka er mellom 07:00 og 23:00 settes kollektivet på dagmodus.
        send_data("30493","eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1MTkwIn0.-xszm_AFkXDZTnevdjFy9Ivvozdp02EwBSulSWwiOHg",0)
    else:
        
        # Er den ikkje det, settes nattmodus på.
        send_data("30493","eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1MTkwIn0.-xszm_AFkXDZTnevdjFy9Ivvozdp02EwBSulSWwiOHg",1)
    
    # Hver time hentes  
    if date.minute==0:
        
        # Henter inn temperatur og skydekke fra yr.
        print("hey")
        temp,skyer_prosent=vær_data()
        print(skyer_prosent)
        send_data("12767"," eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1OTIyIn0.-QervryV2oRrnpeFFmbx7RGGSLwKmIWhKN3X1mAXCy0",skyer_prosent)
        send_data("20292"," eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1OTIyIn0.-QervryV2oRrnpeFFmbx7RGGSLwKmIWhKN3X1mAXCy0",temp)
    
    # Kjørest hver dag kl 00:00.
    if date.hour==0 and date.minute==0:
       
        # Henter oppdatert strømdata.
        df=pd.read_csv("df_merged.csv")
        df=df.drop("Position",axis=1)
       
        # Sjekker når strømprisen er lavest.
        index_lavest_pris=df["Price_amount"].idcmin()
       
        # Setter vaskemaskin start som timen med lavest strømpris
        vaskemaskin_start=datetime.strptime(df["PriceTime"].iloc[index_lavest_pris],"%Y-%m-%d %H:%M:%S")
    
    # Starter vaskemaskin når strømprisen er lavest.
    if vaskemaskin_start.hour==date.hour:
        send_data("22356","eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1OTIyIn0.-QervryV2oRrnpeFFmbx7RGGSLwKmIWhKN3X1mAXCy0" ,1)
    else:
        send_data("22356","eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1OTIyIn0.-QervryV2oRrnpeFFmbx7RGGSLwKmIWhKN3X1mAXCy0" ,0)
    sleep(60)
    
        
    