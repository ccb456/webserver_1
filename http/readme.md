<!--
 * @Author       : mark
 * @Date         : 2020-06-19
 * @copyleft GPL 2.0
--> 
HTTP

## 表单数据编码格式

application/x-www-form-urlencoded 是 HTTP 协议中一种常用的表单数据编码格式，主要用于通过 POST 请求提交简单的键值对数据（如 HTML 表单中的用户输入）。
## 核心特点

### 1. 数据格式：
数据以 key1=value1&key2=value2 的键值对形式组织，例如表单中的用户名和密码会被编码为 username=admin&
password=123456。

### 2. 特殊字符编码：
空格会被替换为 + 或 %20；

非 ASCII 字符（如中文、符号）会被转换为 %XX 形式的 URL 编码（十六进制表示），例如 @ 会编码为 %40；

键和值之间用 = 分隔，键值对之间用 & 分隔。

### 3.适用场景：
适用于提交简单文本数据（如登录表单、搜索框输入等），不适合传输二进制数据（如文件上传，此时应使用 multipart/form-data 类型）。


## 示例：
若表单提交 username=张&password=ab+c，编码后的数据为 username=%E5%BC%A0&password=ab%2Bc，解析后会得到 post_ = {"username": "张", "password": "ab+c"}。