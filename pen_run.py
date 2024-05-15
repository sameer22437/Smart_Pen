import requests
import time
import easyocr



import pytesseract
from autocorrect import Speller

from PIL import Image
from io import BytesIO

from nltk.corpus import wordnet


spell = Speller()

main_url='http://192.168.131.165/'
route_image='saved_photo'
route_type="saved_type"
route_status="status_tell"
route_text="receive_text"

reader = easyocr.Reader(['en'])


def do():
    response = requests.get(main_url+route_image)
    response_type= requests.get(main_url+route_type)
    res_type=response_type.content
    print(res_type)
    if response.status_code == 200:
        

        image_data = response.content
        image = Image.open(BytesIO(image_data))
        rotated_image = image.rotate(90, expand=False) 

        with open("pen_img.jpg", 'wb') as file:
            rotated_image.save(file)
            
        rotated_image.show()
            
        payload = {"message": '101'}
        result = reader.readtext("pen_img.jpg")
        print("Recognized Text using tesseract:",pytesseract.image_to_string(image,lang='eng',config='--psm 1 --oem 1'))
        print("Recognized Text ocr:")
        a=False
        if(res_type==b'1'):
            for detection in result:
                print(detection[1])
                up_string=""
                final_up_string=""
                for i in detection[1]:
                    if i !=' ':
                        up_string=up_string+i

                dict = wordnet.synsets(up_string)
                if(wordnet.synsets(up_string)):
                    print(up_string.upper())  # Extracted text
                    dict = wordnet.synsets(up_string)
                    final_up_string=up_string
                else:
                    print(spell(up_string).upper())  # Extracted text
                    dict = wordnet.synsets(spell(up_string))
                    final_up_string=spell(up_string)

                print(dict)
                if(dict):
                    meanD=dict[0].definition()
                    print(meanD)
                    payload={"message": '111',"modeType":res_type,"wordDetected":up_string,"wordCorrect":final_up_string,"wordMeaning":meanD}
                    a=True
                    break
            if a==False:
                payload={"message": '101',"modeType":"NA","wordDetected":"NA","wordCorrect":"NA","wordMeaning":"NA"}
        elif res_type==b'2':
            cal=0
            a_num=0
            if(detection[1]):
                print(detection[1])
                for i in detection[1]:
                    if i=='+' or i == "x" or i== '÷' or i== '-':
                        if(i=='+'):
                            cal=cal+a_num
                        elif(i=="-"):
                            cal=cal-a_num
                        elif(i=="÷"):
                            cal=cal/a_num
                        elif(i=="-"):
                            cal=cal-a_num
                        a_num=0
                    else:
                        a_num=a_num*10+int(i)
                    print(cal)
                payload={"message": '201',"modeType":"NA","wordDetected":"NA","wordCorrect":"NA","wordMeaning":"NA"}
            else:
                payload={"message": '201',"modeType":"NA","wordDetected":"NA","wordCorrect":"NA","wordMeaning":"NA"}
        try:
            response = requests.post(main_url + route_text, data=payload, timeout=10)
            print(response.text)
        except Exception as e:
            print("An error occurred:", e)

    else:
        print("Failed to retrieve the image")


while True:
    check = requests.get(main_url + route_status)
    if check.status_code == 200:
        data = check.content
        print(data)
        if data == b'true': 
            do()
    else:
        print("Failed to retrieve server status")

    time.sleep(1)