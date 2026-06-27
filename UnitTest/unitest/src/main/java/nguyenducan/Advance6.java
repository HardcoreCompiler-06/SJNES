package nguyenducan;

import java.time.LocalDate;
import java.time.Period;

public class Advance6 {
    public int tinhTuoi(int ngay, int thang, int nam) {
        try {
            LocalDate ngaySinh = LocalDate.of(nam, thang, ngay);
            LocalDate ngayHienTai = LocalDate.now();
            int tuoi = Period.between(ngaySinh, ngayHienTai).getYears();
            if (tuoi < 0) return -1;
            return tuoi;
        } catch (Exception e) {
            return -1;
        }
    }
}