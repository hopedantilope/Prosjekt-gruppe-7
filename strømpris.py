# -*- coding: utf-8 -*-
"""
Created on Wed May  5 13:08:45 2021

@author: haava
"""
from time import time,sleep
import datetime
import json 
import requests
import pandas as pd
import numpy as np


# Token til CoT
token_1="eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1OTIyIn0.-QervryV2oRrnpeFFmbx7RGGSLwKmIWhKN3X1mAXCy0"
token_2="eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1OTY2In0.jpA9BqhHik3DZACJT-aMcrSunlvQeyid9snyh6WxPTA"
token_3="eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1MTkwIn0.-xszm_AFkXDZTnevdjFy9Ivvozdp02EwBSulSWwiOHg"

# Funksjon som kjøres ein gong ved start
def startup():
    
    # lager ein dataframe av strom.csv
    df_start=pd.read_csv("strom.csv")
    
    # Hvis dataframen er tom, setter kwt,pris,total_strøm_produsert=0,0,0, Som er variabler som brukt senerei programmet.
    if len(df_start.index)==0:
        return 0,0,0
    
    # Det samme skjer hvis den siste månen registrert ikke er denne månen.
    if df_start["Month"].iloc[-1]!=datetime.datetime.now().month:
        return 0,0,0
    
    # Hvis den siste rader korrensponderer med månen, hentes dei lagra variablane inn.
    else:
        return df_start.iloc[-1]["kwt"],df_start.iloc[-1]["pris"],df_start.iloc[-1]["produsert"]
    

def hent_data(KEY_1,TOKEN_1):
    
    # Lager dictonary
    data_1={'Key':'0','Value':0,'Token':'0'} 
    
    # Definerer key og tolken
    data_1['Key']=KEY_1 
    data_1['Token']=TOKEN_1
    
    payload = data_1
    response=requests.get('https://circusofthings.com/ReadValue',params=payload)

    
    datahandling=json.loads(response.content)  
    return datahandling["Value"]
    
def send_data(KEY_1,TOKEN_1,VALUE_1):
    
   
    # Lager dictonary 
    data_1={'Key':'0','Value':0,'Token':'0'} 
    
    # definer key, value og token
    data_1['Key']=KEY_1 
    data_1['Value']=VALUE_1
    data_1['Token']=TOKEN_1
 
    """" Legger inn PUT request til CoT """
 
    response=requests.put('https://circusofthings.com/WriteValue',
                          data=json.dumps(data_1),headers={'Content-Type':'application/json'})

# Logger strømpriser hver månad
def logg_data():
    global kwt,pris,total_strøm_produsert
    df_logg=pd.read_csv("strom.csv")
    
    # Sjekker om det er ein ny måne, hvis det er det lages det en ny rad.
    if date.day==1 and date.hour==0:
        new_row={'Year':date.year, 'Month':date.month, 'kwt':kwt, 'pris':pris, 'produsert':total_strøm_produsert}
        df_logg = df_logg.append(new_row, ignore_index=True)
        df_logg.to_csv(r"strom.csv",index=False,header=True)
        
        # Setter variablene til 0 siden det er en ny måne
        kwt,pris,total_strøm_produsert=0,0,0
        
    # Hvis det ikke er en ny måne, blir dei nye verdiene logget.
    if len(df_logg.index)==0:
        new_row={'Year':date.year, 'Month':date.month, 'kwt':kwt, 'pris':pris, 'produsert':total_strøm_produsert}
        df_logg = df_logg.append(new_row, ignore_index=True)
        df_logg.to_csv(r"strom.csv",index=False,header=True)
   
    # Hvis siste måne i datasettet er samme måne som nå  
    elif df_logg["Month"].iloc[-1]==date.month:
        df_logg.iloc[-1]=[date.year,date.month,kwt,pris,total_strøm_produsert]
        df_logg.to_csv(r"strom.csv",index=False,header=True)
     
    # ellers opprettes det en ny rad.
    else:
        new_row={'Year':date.year, 'Month':date.month, 'kwt':kwt, 'pris':pris, 'produsert':total_strøm_produsert}
        df_logg = df_logg.append(new_row, ignore_index=True)
        df_logg.to_csv(r"strom.csv",index=False,header=True)
        
def kwt_til_kr(kwt):
    global total_strøm_produsert,pris
    nettleie=45.24
    
    # Henter dagens strømpriser
    df=pd.read_csv("df_merged.csv")
    df=df.drop("Position",axis=1)
    
    # Finner strømprisene for gjeldene time
    strøm_pris=df["Price_amount"].iloc[date.hour]
    
    # Henter ut solcelle produksjonen utifra en formel.
    total_strøm_produsert+=solcelle_prod()
    
    # Rekner ut pris som blir brukt - produsert * strøm_pris
    pris=((nettleie + strøm_pris)*(kwt-total_strøm_produsert))/100
    
    # Sender så data til COT
    send_data("17952",token_2,pris)
    send_data("25366",token_2,total_strøm_produsert)
    
    # Logger data
    logg_data()
    
def solcelle_prod():

    dayOfYear = datetime.datetime.now().timetuple().tm_yday    # Setter dayOfYear til dagen i året (0-366)
    hourOfDay = datetime.datetime.now().timetuple().tm_hour    # Setter hourOfDay til nåværende time (0-23)
    iterationHours = [hourOfDay, hourOfDay-0.2, hourOfDay-0.4, hourOfDay-0.6, hourOfDay-0.8, hourOfDay-1]   #Setter seks punkter for iterering via trapesmetoden
    cloudPercent = hent_data("12767"," eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1OTIyIn0.-QervryV2oRrnpeFFmbx7RGGSLwKmIWhKN3X1mAXCy0")     #Setter cloudPercent til skydekke (0-1)
    average_list = []       # Tom liste for svarene av trapesmetoden
    
    for t in iterationHours:
        sunMaxHeight = -23.5*np.cos((2 * np.pi * dayOfYear / 365)) + 26.5  # sunMaxHeight: solas max høyde gitt dagen i året (i grader)
        sunCurve = sunMaxHeight - (26.5*(1 + np.cos((2*np.pi * t / 24))))  #sunCurve: solas bane i løpet av en dag gitt timen på dagen (i grader fra ekvatorlinja)
        sunCurveRad = sunCurve * np.pi / 180             # sunCurveRad: theta omgjort til radianer
        panelEfficiency = np.sin((sunCurveRad))           # panelEfficiency: Effektivitetsnivå basert på vinkel mellom sola og solcellepanelet
        powerProduction = 25*panelEfficiency*(1-(0.008 * cloudPercent))            #powerProduction: Produsert effekt til et hvert tid (KW)
        
        if(powerProduction < 0):        # Setter strømproduksjon til 0 dersom den er negativ siden vi ikke lagrer overfladisk strøm
            powerProduction = 0         
            
        average_list.append(powerProduction)    # Legger strømproduksjon til i en liste
    
    return sum(average_list)/6          # Returnerer gjennomsnittsverdigen av lista. Til sammen seks punkter. Dette gir greit estimat

print(solcelle_prod())
     

def forbruk():
    global kwt
    
    
    # Henter all nødvendig data fra COT
    oppvaskemaskin_på=hent_data("22356",token_1)
    vaskemaskin_på=hent_data("31572",token_2)
    dusj_på=hent_data("19570",token_2) 
    bad_varme=hent_data("1059",token_3)
    soverom_varme=hent_data("14964",token_3)
    kjøkken_varme=hent_data("27481",token_3)
    stue_varme=hent_data("15097",token_3)
    
    # Hvis verdien er 1 legger vi til antall kwt per minutt.
    if dusj_på==1:
        kwt+=0.6
    
    if vaskemaskin_på==1:
        kwt+=1.89/60
    
    if oppvaskemaskin_på==1:
        kwt+=0.8/60
    
    if bad_varme==1:
        kwt+=2/60
        
    if kjøkken_varme==1:
        kwt+=2/60
        
    if stue_varme==1:
        kwt+=2/60
    
    if soverom_varme==1:
        kwt+=2/60
        
    # Sover 1 sekund for å sikre at funksjonen ikkje kjøres 2 ganger på 1 min.
    sleep(1)
    
    # Når det da har gått 1 time kjøres logg on sendefunksjonene.
    if date.minute==0:
        print(kwt)
        kwt+=0.5
        send_data("27425",token_2,kwt)
        kwt_til_kr(kwt)
    

        
kwt,pris,total_strøm_produsert=startup()

while True:
    date=datetime.datetime.now()
    
    # Hver minutt kjøres forbruk
    if date.second==0:
        forbruk()
    sleep(0.1)   
        
        
