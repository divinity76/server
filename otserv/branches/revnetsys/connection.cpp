//////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//////////////////////////////////////////////////////////////////////
#include "otpch.h"

#include "connection.h"
#include "protocol.h"
#include "outputmessage.h"
#include "protocol79.h"
#include "protocollogin.h"
#include "status.h"
#include "tasks.h"
#include "scheduler.h"

#include <boost/bind.hpp>

#ifdef __DEBUG_NET__
#define PRINT_ASIO_ERROR(desc) \
	std::cout << "Error: [" << __FUNCTION__ << "] " << desc << " - Error: " <<  \
		error.value() << " Desc: " << error.message() << std::endl;
#else
#define PRINT_ASIO_ERROR(desc)
#endif

void Connection::closeConnection()
{
	std::cout << "Connection::closeConnection" << std::endl;
	OTSYS_THREAD_LOCK_CLASS lockClass(m_connectionLock);
	if(m_closeState != CLOSE_STATE_NONE)
		return;
	
	m_closeState = CLOSE_STATE_REQUESTED;
	
	Scheduler::getScheduler().addEvent(
		createSchedulerTask(1000, boost::bind(&Connection::closeConnectionTask, this)));
}

void Connection::closeConnectionTask()
{
	std::cout << "Connection::closeConnectionTask" << std::endl;
	OTSYS_THREAD_LOCK_CLASS lockClass(m_connectionLock);
	if(m_closeState != CLOSE_STATE_REQUESTED){
		std::cout << "Error: [Connection::closeConnectionTask] m_closeState = " << m_closeState << std::endl;
		return;
	}
	
	m_closeState = CLOSE_STATE_CLOSING;
	
	if(m_protocol){
		Dispatcher::getDispatcher().addTask(
			createTask(boost::bind(&Protocol::deleteProtocolTask, m_protocol)));
		m_protocol->setConnection(NULL);
		m_protocol = NULL;
	}
	
	closingConnection();
}

void Connection::acceptConnection()
{
	// Read size of te first packet
	boost::asio::async_read(m_socket,
		boost::asio::buffer(m_msg.getBuffer(), NetworkMessage::header_length),
		boost::bind(&Connection::parseHeader, this, boost::asio::placeholders::error));
	m_pendingRead++;
}

void Connection::parseHeader(const boost::system::error_code& error)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(m_connectionLock);
	m_pendingRead--;
	if(m_closeState == CLOSE_STATE_CLOSING){
		closingConnection();
		return;
	}
	
	int32_t size = m_msg.decodeHeader();
	if(!error && size > 0 && size < NETWORKMESSAGE_MAXSIZE - 16){
		// Read packet content
		m_msg.setMessageLength(size + NetworkMessage::header_length);
		boost::asio::async_read(m_socket, boost::asio::buffer(m_msg.getBodyBuffer(), size),
			boost::bind(&Connection::parsePacket, this, boost::asio::placeholders::error));
		m_pendingRead++;
	}
	else{
		if(error == boost::asio::error::operation_aborted){
			//
		}
		else{
			PRINT_ASIO_ERROR("Reading header");
			closeConnection();
		}
	}
}

void Connection::parsePacket(const boost::system::error_code& error)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(m_connectionLock);
	m_pendingRead--;
	if(m_closeState == CLOSE_STATE_CLOSING){
		closingConnection();
		return;
	}
	
	if(!error){
		// Protocol selection
		if(!m_protocol){
			// Protocol depends on the first byte of the packet
			uint8_t protocolId = m_msg.GetByte();
			switch(protocolId){
			case 0x01: // Login server protocol
				m_protocol = new ProtocolLogin(this);
				break;
			case 0x0A: // World server protocol
				m_protocol = new Protocol79(this);
				break;
			case 0xFE: // Admin protocol
				break;
			case 0xFF: // Status protocol
				m_protocol = new ProtocolStatus(this);
				break;
			default:
				// No valid protocol
				closeConnection();
				break;
			}
		}
		
		// Send the packet to the current protocol
		m_protocol->onRecvMessage(m_msg);
		
		// Wait to the next packet
		boost::asio::async_read(m_socket,
			boost::asio::buffer(m_msg.getBuffer(), NetworkMessage::header_length),
			boost::bind(&Connection::parseHeader, this, boost::asio::placeholders::error));
		m_pendingRead++;
	}
	else{
		if(error == boost::asio::error::operation_aborted){
			//
		}
		else{
			PRINT_ASIO_ERROR("Reading packet");
			closeConnection();
		}
	}
}

void Connection::send(OutputMessage* msg)
{
	std::cout << "Connection::send init" << std::endl;
	OTSYS_THREAD_LOCK_CLASS lockClass(m_connectionLock);
	if(m_closeState == CLOSE_STATE_CLOSING)
		return;
	
	if(m_protocol){
		m_protocol->onSendMessage(msg);
	}
	
	if(m_pendingWrite == 0){
		std::cout << "Connection::send " << msg->getMessageLength() << std::endl;
		internalSend(msg);
	}
	else{
		std::cout << "Connection::send Adding to queue " << msg->getMessageLength() << std::endl;
		m_outputQueue.push_back(msg);
	}
}

void Connection::internalSend(OutputMessage* msg)
{
	boost::asio::async_write(m_socket,
		boost::asio::buffer(msg->getOutputBuffer(), msg->getMessageLength()),
		boost::bind(&OutputMessagePool::writeHandler, msg, boost::asio::placeholders::error));
		
	m_pendingWrite++;
}


uint32_t Connection::getIP()
{
	//Ip is expressed in network byte order
	boost::system::error_code error;
	const boost::asio::ip::tcp::endpoint endpoint = m_socket.remote_endpoint(error);
	if(!error){
		return htonl(endpoint.address().to_v4().to_ulong());
	}
	else{
		PRINT_ASIO_ERROR("Getting remote ip");
		return 0;
	}
}


void Connection::onWriteOperation(const boost::system::error_code& error)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(m_connectionLock);
	std::cout << "onWriteOperation" << std::endl;
	
	if(!error){
		if(m_pendingWrite > 0){
			m_pendingWrite--;
			if(m_outputQueue.size() != 0){
				OutputMessage* msg = m_outputQueue.front();
				m_outputQueue.pop_front();
				std::cout << "Connection::onWriteOperation send " << msg->getMessageLength() << std::endl;
				internalSend(msg);
			}
		}
		else{
			std::cout << "onWriteOperation. Error 1" << std::endl;
			// Error. Pending operations counter is 0, but we are getting a
			// notification!!
		}
	}
	else{
		PRINT_ASIO_ERROR("Writting");
		closeConnection();
	}
	
	if(m_closeState == CLOSE_STATE_CLOSING){
		closingConnection();
		return;
	}
}

void Connection::closingConnection()
{
	std::cout << "Connection::closingConnection" << std::endl;
	if(m_pendingWrite == 0){
		if(!m_socketClosed){
			std::cout << "Closing socket" << std::endl;
			boost::system::error_code error;
			m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, error);
			if(error){
				PRINT_ASIO_ERROR("Shutdown");
			}
			m_socket.close(error);
			m_socketClosed = true;
			if(error){
				PRINT_ASIO_ERROR("Close");
			}
		}
		if(m_pendingRead == 0){
			std::cout << "Deleting Connection" << std::endl;
			delete this;
		}
	}
}
