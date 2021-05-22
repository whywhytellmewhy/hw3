# hw3
Created on 110.5.7

# Q1: How to setup and run your program？
A1: 
This homework is so difficult that I spent almost a whole weekend to finish it before exam 2 was around the corner. There are two main functions in the program: 
1. gesture mode: Use machine learning in lab8 to identify different gestures (to differentiate them) to make a choice between different angles. 這些角度會
在之後的tilt mode中當作判斷是否超限的標準。我利用lab8中的2個範例手勢(ring及slope)，再加上當時我訓練機器得到的 "heart" 手勢，共三個手勢，
分別對應到傾斜角25度、50度、75度的標準角度。當使用者在螢幕(screen)上打上/GESTURE/run的RPC指令(command)後，程式呼叫gesture函式，這個函式會一方面呼叫
gesture_thread這個thread，並且同時讓myled2閃爍，以讓使用者得知開始gesture mode。在gesture_thread函式中，即是讓板子判斷現在的手勢維和，
並"可更新地"將手勢顯示在螢幕上，並將其所對應到的角度標準值列印(print)在uLCD的螢幕上，也同時呼叫gesture_confirm_thread這個函式。當使用者按下
"USER_BUTTON"的按鈕(使其輸入值變成0)時，會使得gesture_confirm_thread正式進入迴圈，而停止前面判斷手勢的程式，以避免耗費運算資源。gesture_confirm_thread的功能
也確認角度標準值為何，並將她特別儲存起來，也在uLCD的螢幕上顯示出確認(confirm)後的值，接著再與PC端(PC上運行的python程式)作Wifi連線，
並將確認後的角度標準值傳送(publish)到PC端，當python程式接收到封包(packet)後，它會將這個直儲存起來，並傳送/STOPGESTURE/run的RPC指令
給板子，這會使板子重新以軟體方式進行reset，而得以回到RPC loop的狀態(state)。但此時由於reset，因此板子所儲存的角度確認值已不見，因此
必須再從python端傳送MQTT packet (內含確認過的角度) 給mbed。 
2. tilt mode: 在至少經過一次的gesture mode，而有(至少)一個角度標準值之後，即可進入tilt模式(mode)。當使用者在螢幕中打上/TILT/run的RPC指令後
，即可進入tilt模式。程式會呼叫tilt函式，與gesture函式類似地，tilt函式會一面使myled2閃爍來代表mode開始進行，另一方面會呼叫tilt_thread的函式，
此函示會使板子上的加速度感應器(accelerometer)開始啟動(initialize)，並且將myled3的值從0改變成1，使使用者了解此時為take reference acceleration的時間。
在USER_BUTTON被按下並放開時程式會呼叫take_reference_a_midway的函式，它會呼叫一個eventqueue (在此稱作button_queue)，為了防止在interrupt所呼叫的
函式中會有printf等等的指令出現，而這個eventqueue則會呼叫take_reference_a 這個函式(其中的"a"表示加速度)，在這個函式中，會將此時的加速度感應器所測量出的加速度
值，依照x、y、z軸方向的分量分別儲存在reference_XYZ的陣列中，此時將myled3設回0，並在螢幕上print出"Take_reference_a_success!!"的字樣，表示
已成功收集到參考加速度值。接著就可以開始測量每個時間點所對應到的加速度值以及由此計算出傾斜角度了！因此程式呼叫tilt_thread_any_point的韓式
，這個函式運作時會先連上跟PC端之間的Wifi網路，以利即時輸送(transmit)超過標準角度值的數據到PC端，

# Q2: What are the results？
A2:在python的執行螢幕上會以 
 
\---------------- 
 
(資料、數據內容) 
 
\---------------- 
 
的方式區隔出兩個結果(其餘部分為log訊息)： 
1.確認的角度標準值 
2.超過角度標準值的10筆傾斜(tilt)數據(數據內含超過的數據筆數值、x軸方向加速度測量值、y軸方向加速度測量值、z軸方向加速度測量值、計算出的傾斜角度) 
