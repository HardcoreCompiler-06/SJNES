package nguyenquyetchien;

import java.time.LocalDate;
import java.time.Period;
public class Advance6 {
/*Tính số tuổi dựa vào ngày tháng năm sinh
* Nếu nhập dữ liệu sai sẽ trả về -1
* VD: 12/1/1999 -> 21, 12/5/1999 -> 20
*/
public int tinhTuoi(int ngay, int thang, int nam) {
LocalDate ngaySinh =LocalDate.of(nam, thang, ngay);
LocalDate ngayHienTai = LocalDate.now();
return Period.between(ngaySinh, ngayHienTai).getYears();
 }
} 
