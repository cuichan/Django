namespace cpp save_service

service Save{
    # username: myserver的名称
    # password:服务器密码的md5sun前八位
    # 成功0，失败1
    # 
    i32 save_data(1: string username,2: string password,3: i32 player1_id,4: i32 player2_id)
}
