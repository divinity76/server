AUTOMAKE_OPTIONS = foreign

noinst_PROGRAMS = otserv


AM_CXXFLAGS = $(XML_CPPFLAGS) $(OTSERV_FLAGS) $(LUA50_CFLAGS) $(MYSQL_FLAGS) $(PROFILER_FLAGS) -D_THREAD_SAFE -D_REENTRANT -Werror
otserv_LDADD = $(XML_LIBS) $(LUA50_LIBS) $(BOOST_LIBS) -ldl
if USE_MYSQL
otserv_SOURCES = account.cpp container.cpp creature.cpp game.cpp ioaccount.cpp \
	iomapxml.cpp ioplayer.cpp ioplayerxml.h ioplayerxml.cpp ioaccountxml.cpp ioaccountxml.h \
	item.cpp items.cpp luascript.cpp magic.cpp map.cpp monster.cpp networkmessage.cpp npc.cpp \
	otserv.cpp player.cpp position.cpp protocol.cpp protocol76.cpp spells.cpp \
	status.cpp thing.cpp tile.cpp tools.cpp account.h bootstrap.sh config.h const76.h \
	container.h creature.h definitions.h game.h ioaccount.h depot.cpp depot.h \
	iomap.h iomapxml.h ioplayer.h item.h items.h teleport.cpp teleport.h \
	luascript.h magic.h map.h monster.h networkmessage.h npc.h otsystem.h player.h \
	position.h protocol.h protocol76.h scheduler.h spells.h stamp-h status.h \
	tasks.h templates.h texcept.h thing.h tile.h tools.h spawn.cpp spawn.h \
	actions.h actions.cpp scheduler.cpp ioplayersql.cpp ioplayersql.h ioaccountsql.cpp ioaccountsql.h \
	commands.cpp commands.h database.cpp database.h md5.cpp md5.h fileloader.cpp fileloader.h \
	chat.cpp chat.h logger.h logger.cpp iomapotbm.h iomapotbm.cpp monsters.h monsters.cpp \
	trashholder.cpp trashholder.h mailbox.cpp mailbox.h waitlist.cpp waitlist.h \
	house.cpp house.h housetile.cpp housetile.h iomapserialize.cpp iomapserialize.h \
	iomapserializesql.cpp iomapserializesql.h iomapserializexml.cpp iomapserializexml.h
else
otserv_SOURCES = account.cpp container.cpp creature.cpp game.cpp ioaccount.cpp \
	ioaccountxml.cpp iomapxml.cpp ioplayer.cpp ioplayerxml.cpp \
	item.cpp items.cpp luascript.cpp magic.cpp map.cpp monster.cpp networkmessage.cpp npc.cpp \
	otserv.cpp player.cpp position.cpp protocol.cpp protocol76.cpp spells.cpp \
	status.cpp thing.cpp tile.cpp tools.cpp account.h bootstrap.sh config.h const76.h \
	container.h creature.h definitions.h game.h ioaccount.h ioaccountxml.h \
	iomap.h iomapxml.h ioplayer.h ioplayerxml.h item.h items.h \
	luascript.h magic.h map.h monster.h networkmessage.h npc.h otsystem.h player.h \
	position.h protocol.h protocol76.h scheduler.h spells.h stamp-h status.h \
	tasks.h templates.h texcept.h thing.h tile.h tools.h spawn.cpp spawn.h \
	actions.h actions.cpp scheduler.cpp commands.cpp commands.h depot.cpp depot.h \
	md5.cpp md5.h fileloader.cpp fileloader.h chat.cpp chat.h logger.cpp logger.h \
	iomapotbm.h iomapotbm.cpp monsters.h monsters.cpp teleport.cpp teleport.h \
	trashholder.cpp trashholder.h mailbox.cpp mailbox.h waitlist.cpp waitlist.h \
	house.cpp house.h housetile.cpp housetile.h iomapserialize.cpp iomapserialize.h \
	iomapserializexml.cpp iomapserializexml.h
endif