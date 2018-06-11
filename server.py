# -*- coding: utf-8 -*-
import paho.mqtt.client as mqtt
import telegram
import telegram.ext
import threading
import json

MQTT_USER_NAME = ''
MQTT_PASSWORD = ''
BOT_TOKEN = ''
DEV_IDS = dict()
COMMAND_INFO_LIST = [('cadastrardevid',u'Cadastra DevId de despositivo, toda vez que existir alguma notificação sobre este DevId a mesma será enviada a você.')]

DEV_IDS_LOCK = threading.Lock()

def handlerStart(bot, update):
	print 'handlerStart'
	msgStart = u'Lista de Comandos:\n' + '\n'.join([u'{} - {}'.format(x[0],x[1]) for x in COMMAND_INFO_LIST])
	bot.send_message(chat_id = update.message.chat_id, text = msgStart)

def handlerCadastrarDevId(bot, update, args):
	if len(args) > 0:
		DEV_IDS_LOCK.acquire()
		print 'handlerCadastrarDevId'
		devId = args[0]
		lista = DEV_IDS.get(devId, None)

		if lista is not None:
			if update.message.chat_id in lista:
				bot.send_message(chat_id = update.message.chat_id, text = u'DevId {} já está cadastrado para você'.format(devId))
			else:
				lista.add(update.message.chat_id)
				bot.send_message(chat_id = update.message.chat_id, text = u'DevId {} cadastrado com sucesso'.format(devId))	
		else:
			DEV_IDS[devId] = set([update.message.chat_id])		
			bot.send_message(chat_id = update.message.chat_id, text = u'DevId {} cadastrado com sucesso'.format(devId))	
		
		DEV_IDS_LOCK.release()

def startTelegramUpdater():
	updater = telegram.ext.Updater(token = BOT_TOKEN)
	updater.dispatcher.add_handler(telegram.ext.CommandHandler('start', handlerStart))
	updater.dispatcher.add_handler(telegram.ext.CommandHandler('help', handlerStart))
	updater.dispatcher.add_handler(telegram.ext.CommandHandler('cadastrardevid', handlerCadastrarDevId, pass_args = True))
	return updater

def startMqttConnection(jobQueue):
	mqttClient = mqtt.Client(client_id="client123456789TelegramBot", clean_session = True, userdata = jobQueue)
	mqttClient.username_pw_set(MQTT_USER_NAME, password = MQTT_PASSWORD)
	mqttClient.on_message = mqttOnMessage	
	mqttClient.connect('thethings.meshed.com.au', port = 1883, keepalive = 60, bind_address = "")
	mqttClient.subscribe('+/devices/+/up', qos = 0)
	return mqttClient

def callbackSendMenssage(chat_id,text):
	def f(bot, job):
		bot.send_message(chat_id = chat_id, text = text)
	return f

def mqttOnMessage(client, userdata, message):
	msgDict = json.loads(message.payload)	
	devId = msgDict["dev_id"]
	counter = int(msgDict["payload_fields"]["counter"])
	print devId, counter
	if counter == 1: # invasao
		DEV_IDS_LOCK.acquire()
		chatIds = DEV_IDS.get(devId, None)
		if chatIds is not None:
			jobQueue = userdata
			msg = u'DevId {} foi invadido!!!!'.format(devId)
			for chatId in chatIds:				
				jobQueue.run_once(callbackSendMenssage(chatId,msg),0) 
		DEV_IDS_LOCK.release()	

if __name__ == '__main__':
	updater = startTelegramUpdater()
	jobQueue = updater.job_queue
	updater.start_polling()

	mqttClient = startMqttConnection(jobQueue)
	mqttClient.loop_forever()
