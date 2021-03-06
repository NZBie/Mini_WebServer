#include "../BLL.h"

bool getFileList(const Value& params, Value& rpsJson) {

	string token = params["token"].asString();
	int uid = parse_token(token);
	
	string fid = params["fid"].asString();

	if(fid.size() == 0) {
		string rootID = "select rootID from info where\'" + to_string(uid) + "\'";
		MYSQL_RES* result = execute_query(rootID);
		MYSQL_ROW row = mysql_fetch_row(result);
		fid = row[0];
	}

	// 当前文件夹信息
	string folder_query = "select * from folder where fid=\'" + fid + "\'";
	MYSQL_RES* result = execute_query(folder_query);
	MYSQL_ROW row = mysql_fetch_row(result);
	rpsJson["fid"] = row[0];
	rpsJson["fName"] = row[2];

	// 当前目录下的文件夹信息
	string folders_query = "select * from folder where parID=\'" + fid + "\'";
	result = execute_query(folders_query);
	while(row = mysql_fetch_row(result)) {
		if(strcmp(row[4], "1") == 0) continue;
		Value folder;
		folder["fid"] = row[0];
		folder["fName"] = row[2];
		folder["size"] = row[3];
		folder["modifyTime"] = row[5];
		folder["createTime"] = row[6];
		folder["isFolder"] = true;

		rpsJson["fileList"].append(folder);
	}

	// 当前目录下的文件信息
	string files_query = "select * from file where parID=\'" + fid + "\'";
	result = execute_query(files_query);
	while(row = mysql_fetch_row(result)) {
		if(strcmp(row[4], "1") == 0) continue;
		Value file;
		file["fid"] = row[0];
		file["fName"] = row[2];
		file["extension"] = row[3];
		file["size"] = row[4];
		file["modifyTime"] = row[6];
		file["createTime"] = row[7];
		file["isFolder"] = false;

		rpsJson["fileList"].append(file);
	}

	rpsJson["msg"] = "ok";

	return true;
}