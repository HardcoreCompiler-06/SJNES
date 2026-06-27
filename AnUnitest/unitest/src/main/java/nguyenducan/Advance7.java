package nguyenducan;

import java.util.Calendar;

public class Advance7 {
    public int tinhThu(int ngay, int thang, int nam) {
        try {
            if (ngay < 1 || ngay > 31) return 0;
            if (thang < 1 || thang > 12) return 0;
            if (nam < 0) return 0;
            Calendar cal = Calendar.getInstance();
            cal.setLenient(false);
            cal.set(nam, thang - 1, ngay);
            cal.getTime();
            return cal.get(Calendar.DAY_OF_WEEK);
        } catch (Exception e) {
            return 0;
        }
    }
}