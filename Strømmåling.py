#Importering 
import requests
import pandas as pd
import datetime
import lxml.etree as ET
import schedule
from time import time,sleep
 
#Def som henter inn xml fil med strømpriser fra entsoe
#Blir behandlet med pandas og gjort om til csv fil
def export_to_bq(t):
    
    # Parametere
    securityToken= 'e5fd0837-384d-4d8d-aa6a-e8d6089d769f'
    param = "&documentType=A44&in_Domain="
    in_Domain = "10YNO-3--------J"
    out_Domain = "10YNO-3--------J"
 
 
    #Finner dato for dagens dag og imorgen dagen, forså å sette klokkeslett til datoene
    today = datetime.date.today()
    periodStart = today
    periodStart = str(periodStart)
    periodStart = periodStart.replace("-", "")
    periodStart = periodStart + "2300"
 
    periodEnd = today + datetime.timedelta(days=1)
    periodEnd = str(periodEnd)
    periodEnd = periodEnd.replace("-", "")
    periodEnd = periodEnd + "2300"
 
    #print("Period Start =", periodStart)
    #print("Period End =", periodEnd)
 
    #URL til å hente inn xml fil fra Entsoe
    url = 'https://transparency.entsoe.eu/api?securityToken=' + securityToken + param + in_Domain + "&out_Domain=" + out_Domain + "&periodStart=" + periodStart + "&periodEnd=" + periodEnd  
    #print(url)
 
    resp = requests.get(url)
 
    #Lagrer en midlertidlig xmlfil som kan bli behandlet i pandas.
    xmlfilename = 'ensoeS' + periodStart + 'E' + periodEnd
    with open('xmlfilename','wb') as f:
        f.write(resp.content)
        #print('***XML file created: ' + xmlfilename + ' ***')
        
    #Variabler:
    tree = ET.parse('xmlfilename')
    root = tree.getroot()
    xpoint = []
    list_time = []
    price_time = {}
    counter = 0
    now = datetime.datetime.now()
    ptime = now.replace(hour=00, minute=00, second=00)
    start = (ptime + datetime.timedelta(days=1)).replace(second=0,microsecond=0)
 
    #Dataframes:
    createdDateTime = ""
    df = pd.DataFrame()
    df2 = pd.DataFrame()
 
    #Finner datapunkter og legge dem til i dictionary
    for child in root:
        
        #Finner child.tag for createdDateTime
        if child.tag == "{urn:iec62325.351:tc57wg16:451-3:publicationdocument:7:0}createdDateTime":
            createdDateTime = child.text
        for series in child:
        #need to go deeper 
        
            for period in series:
            
            #deeeeeeper 
                for point in series:
                    
                    #nesten der, trenger variabler 
                    value = {}
                    pos = ""
                    price_amount = ""
                    for x in point:
                        
                        #Legger til tid til dict. Forså å append det til list_time
                        if counter < 24:
                            if counter == 0:
                                price_time = {'PriceTime': str(start)}
                                list_time.append(price_time)
                        start = start + datetime.timedelta(hours=1)
                        counter +=1
                        price_time = {'PriceTime': str(start)}
                        list_time.append(price_time)
                        
                        #Legger til pos,Price_amount og createdDateTime til dictionary forså å append til en liste. 
                        if x.tag == "{urn:iec62325.351:tc57wg16:451-3:publicationdocument:7:0}position":
                            pos = x.text
                        if x.tag == "{urn:iec62325.351:tc57wg16:451-3:publicationdocument:7:0}price.amount":
                            price_amount = x.text
                        value = {'Position': pos, 'Price_amount': price_amount, 'CreatedDateTime': createdDateTime}
                        if price_amount == "":
                            pass
                        else:
                            xpoint.append(value)
                            
    #Appending til dataframe fra dictionary's          
    for dict in xpoint:
        df = df.append(dict, ignore_index=True)    
    for a in list_time:
        df2 = df2.append(a, ignore_index=True)
 
    #Fiksing av dataframe
    
    df.drop_duplicates(subset="Position", keep = 'first', inplace = True)
    df2.drop_duplicates(subset="PriceTime", keep = 'first', inplace = True)
    df2 = df2.iloc[:24]
    df_merged = df.merge(df2, how='outer', left_index=True, right_index=True)
    df_merged['CreatedDateTime'] = pd.to_datetime(df_merged['CreatedDateTime'])
    df_merged['PriceTime'] = pd.to_datetime(df_merged['PriceTime'])
    df_merged.Position = df_merged.Position.astype('int')
    df_merged.Price_amount = df_merged.Price_amount.astype('float')
    df_merged.set_index('PriceTime', inplace=True)
    
    df_merged=df_merged.drop("CreatedDateTime",axis=1)
    
    #Lager om dataframe til CSV fil.
    df_merged.to_csv(r'C:\Users\haava\Akselerasjon\df_merged.csv')

export_to_bq("i")


while True:
    # Henter tiden
    date=datetime.datetime.now()
    # Kjører funksjonen hver dag 23:55
    if date.hour == 23 and date.minute==55:
        export_to_bq()
    sleep(60)
        
    
     
    
    
    
