# -*- coding: utf-8 -*-
import telegram
import telegram.ext
import time
import sys

BOT_TOKEN = 'token'
DEV_IDS = dict()
COMMAND_INFO_LIST = [('cadastrardevid',u'Cadastra DevId de despositivo, toda vez que existir alguma notificação sobre este DevId a mesma será enviada a você.')]

def handlerStart(bot, update):
	print 'handlerStart'
	msgStart = u'Lista de Comandos:\n' + '\n'.join([u'{} - {}'.format(x[0],x[1]) for x in COMMAND_INFO_LIST])
	print msgStart
	bot.send_message(chat_id = update.message.chat_id, text = msgStart)

def handlerCadastrarDevId(bot, update, args):
	print 'handlerCadastrarDevId'
	devId = args[0]
	lista = DEV_IDS.get(devId, None)

	if lista is not None:
		if update.message.chat_id in lista:
			bot.send_message(chat_id = update.message.chat_id, text = u'DevId {} já está cadastrado para você'.format(devId))
			return None
		lista.add(update.message.chat_id)	
	else:
		DEV_IDS[devId] = set([update.message.chat_id])		
		
	bot.send_message(chat_id = update.message.chat_id, text = u'DevId {} cadastrado com sucesso'.format(devId))	

def startTelegramUpdater():
	updater = telegram.ext.Updater(token = BOT_TOKEN)
	updater.dispatcher.add_handler(telegram.ext.CommandHandler('start', handlerStart))
	updater.dispatcher.add_handler(telegram.ext.CommandHandler('help', handlerStart))
	updater.dispatcher.add_handler(telegram.ext.CommandHandler('cadastrardevid', handlerCadastrarDevId, pass_args = True))
	return updater

def callbackSendMenssage(chat_id,text):
	def f(bot, job):
		bot.send_message(chat_id = chat_id, text = text)
	return f

if __name__ == '__main__':
	updater = startTelegramUpdater()
	jobQueue = updater.job_queue
	updater.start_polling()
	print 'Polling'
	while True:
		time.sleep(10)
		print 'ataques'
		for devId, chatIds in DEV_IDS.items():
			msg = u'DevId {} foi atacado'.format(devId)
			for chatId in chatIds:				
				jobQueue.run_once(callbackSendMenssage(chatId,msg),0)
