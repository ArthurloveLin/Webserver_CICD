#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "sql_connection_pool.h"

using namespace std;

connection_pool::connection_pool()
{
	m_CurConn = 0;
	m_FreeConn = 0;
}

connection_pool *connection_pool::GetInstance()
{
	static connection_pool connPool;
	return &connPool;
}

//构造初始化
void connection_pool::init(string url, string User, string PassWord, string DBName, int Port, int MaxConn, int close_log)
{
	m_url = url;
	m_Port = Port;
	m_User = User;
	m_PassWord = PassWord;
	m_DatabaseName = DBName;
	m_close_log = close_log;

	for (int i = 0; i < MaxConn; i++)
	{
		MYSQL *con = NULL;
		con = mysql_init(con);

		if (con == NULL)
		{
			LOG_ERROR("MySQL Error");
			exit(1);
		}
		
		// 设置MySQL连接选项
		mysql_options(con, MYSQL_SET_CHARSET_NAME, "utf8mb4");
		
		// 设置连接超时和读写超时
		unsigned int timeout = 3600; // 1小时
		mysql_options(con, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
		mysql_options(con, MYSQL_OPT_READ_TIMEOUT, &timeout);
		mysql_options(con, MYSQL_OPT_WRITE_TIMEOUT, &timeout);
		
		con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DBName.c_str(), Port, NULL, 0);

		if (con == NULL)
		{
			LOG_ERROR("MySQL Error: %s", mysql_error(con));
			exit(1);
		}
		
		// 连接成功后设置session级别的wait_timeout
		mysql_query(con, "SET SESSION wait_timeout = 7200"); // 2小时
		mysql_query(con, "SET SESSION interactive_timeout = 7200"); // 2小时
		
		connList.push_back(con);
		++m_FreeConn;
	}

	reserve = sem(m_FreeConn);

	m_MaxConn = m_FreeConn;
}


//当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL *connection_pool::GetConnection()
{
	MYSQL *con = NULL;

	if (0 == connList.size())
		return NULL;

	reserve.wait();
	
	lock.lock();

	con = connList.front();
	connList.pop_front();
	
	// 检查连接是否有效，如果断开则重新连接
	if (mysql_ping(con) != 0)
	{
		LOG_ERROR("MySQL connection lost, attempting to reconnect...");
		mysql_close(con);
		
		// 重新创建连接
		con = mysql_init(con);
		if (con != NULL)
		{
			// 重新设置连接选项
			mysql_options(con, MYSQL_SET_CHARSET_NAME, "utf8mb4");
			
			unsigned int timeout = 3600;
			mysql_options(con, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
			mysql_options(con, MYSQL_OPT_READ_TIMEOUT, &timeout);
			mysql_options(con, MYSQL_OPT_WRITE_TIMEOUT, &timeout);
			
			con = mysql_real_connect(con, m_url.c_str(), m_User.c_str(), m_PassWord.c_str(), 
									m_DatabaseName.c_str(), atoi(m_Port.c_str()), NULL, 0);
			
			if (con != NULL)
			{
				mysql_query(con, "SET SESSION wait_timeout = 7200");
				mysql_query(con, "SET SESSION interactive_timeout = 7200");
				printf("MySQL connection restored successfully\n");
			}
			else
			{
				LOG_ERROR("Failed to restore MySQL connection: %s", mysql_error(con));
				lock.unlock();
				reserve.post();
				return NULL;
			}
		}
		else
		{
			LOG_ERROR("Failed to initialize MySQL connection");
			lock.unlock();
			reserve.post();
			return NULL;
		}
	}

	--m_FreeConn;
	++m_CurConn;

	lock.unlock();
	return con;
}

//释放当前使用的连接
bool connection_pool::ReleaseConnection(MYSQL *con)
{
	if (NULL == con)
		return false;

	lock.lock();

	connList.push_back(con);
	++m_FreeConn;
	--m_CurConn;

	lock.unlock();

	reserve.post();
	return true;
}

//销毁数据库连接池
void connection_pool::DestroyPool()
{

	lock.lock();
	if (connList.size() > 0)
	{
		list<MYSQL *>::iterator it;
		for (it = connList.begin(); it != connList.end(); ++it)
		{
			MYSQL *con = *it;
			mysql_close(con);
		}
		m_CurConn = 0;
		m_FreeConn = 0;
		connList.clear();
	}

	lock.unlock();
}

//当前空闲的连接数
int connection_pool::GetFreeConn()
{
	return this->m_FreeConn;
}

connection_pool::~connection_pool()
{
	DestroyPool();
}

connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *connPool){
	*SQL = connPool->GetConnection();
	
	conRAII = *SQL;
	poolRAII = connPool;
}

connectionRAII::~connectionRAII(){
	poolRAII->ReleaseConnection(conRAII);
}