package nguyenquyetchien;

public class Advance5 {
/*Kiểm tra một số có đối xứng không
* Nếu đối xứng trả về true, ngược lại trả về false
* VD: 112 -> false, 12121 -> true
*/
public boolean kiemTraDoiXung(int number){
StringBuilder xau= new StringBuilder();
String str= number+"";
xau.append(str);
String check= xau.reverse().toString();
if(str.equals(check))
return true;
return false;
}
} 
