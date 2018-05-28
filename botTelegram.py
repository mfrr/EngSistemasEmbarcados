# -*- coding: utf-8 -*-
import telegram
import sys
from random import randint

if __name__ == '__main__':
	bot = telegram.Bot(token='')
	print bot.get_me()
	if len(sys.argv) > 1:
		updates = bot.get_updates(offset = int(sys.argv[1])+1)
	else:
		updates = bot.get_updates()
	
	for u in updates:
		if u.message is not None:
			print 'update_id ',u.update_id,' text ',u.message.text
			#bot.send_message(chat_id=u.message.chat_id, text=u"TF não vou te responder.")
			msgs = [u"TF não vou te responder.",u"Vai ver se eu estou na esquina.",u'Te vira boy.',u'Tas fazendo o que aqui? vai pro zap zap.']
			u.message.reply_text(msgs[randint(0, len(msgs)-1)])


