import cv2



def PalmCreaseRemovalTest1(GrayFrame):
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (13, 13))
    # blurred = cv2.GaussianBlur(GrayFrame, (9, 9), 0)
    Creaseless = cv2.morphologyEx(GrayFrame, cv2.MORPH_CLOSE, kernel)
    return Creaseless

def PalmCreaseRemovalTest2(GrayFrame):
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (21, 21))
    background = cv2.morphologyEx(GrayFrame, cv2.MORPH_CLOSE, kernel)

    suppressed = cv2.subtract(background, GrayFrame)

    return suppressed


ClaheConfig = [
    1.0, (16,16)
]

clahe = cv2.createCLAHE(*ClaheConfig)

rtsp = "rtsp://admin::@192.168.1.168:80/ch0_0.264"
captureEngine = cv2.VideoCapture(rtsp)

while True:
    ret, frame = captureEngine.read()
    if not ret: break

    grayscale = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

    # test = PalmCreaseRemovalTest2(grayscale)

    enhanced = clahe.apply(grayscale)

    cv2.imshow("testing clahe", enhanced)
    

    key = cv2.waitKey(1) & 0xFF

    if key == ord("q"):
            break
    if key == ord("c"):
        ClaheConfig[0] += 1.0
        clahe = cv2.createCLAHE(*ClaheConfig)
    if key == ord("C"):
        ClaheConfig[0] -= 1.0
        clahe = cv2.createCLAHE(*ClaheConfig)

captureEngine.release()
cv2.destroyAllWindows()