package nguyenquyetchien;

import org.junit.jupiter.api.Test;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.CsvSource;
import java.time.DateTimeException;
import static org.junit.jupiter.api.Assertions.*;

public class Test12 {

    // 1. Kiểm thử các trường hợp ngày tháng năm sinh hợp lệ
    // LƯU Ý: Số tuổi sẽ được tính dựa trên mốc năm hiện tại của hệ thống máy tính (Năm 2026)
    @ParameterizedTest(name = "Ngày sinh {0}/{1}/{2} -> Tuổi tính toán mong muốn: {3}")
    @CsvSource({
        "12, 1, 1999, 27",  // Sinh ngày 12/1/1999 -> Đến năm 2026 là 27 tuổi
        "12, 5, 1999, 27",  // Sinh ngày 12/5/1999 -> Đến tháng 6/2026 đã qua sinh nhật nên vẫn là 27 tuổi
        "28, 6, 2026, 0",   // Trường hợp vừa sinh hôm nay (28/06/2026) -> 0 tuổi
        "01, 12, 2025, 0"   // Chưa qua sinh nhật kế tiếp -> 0 tuổi
    })
    void testTinhTuoiHopLe(int ngay, int thang, int nam, int expectedAge) {
        Advance6 advance = new Advance6();
        assertEquals(expectedAge, advance.tinhTuoi(ngay, thang, nam));
    }

    // 2. Kiểm thử trường hợp nhập dữ liệu sai (Ném ra ngoại lệ DateTimeException)
    @Test
    void testTinhTuoiDuLieuSai() {
        Advance6 advance = new Advance6();
        
        // Kiểm tra xem hệ thống có ném ra lỗi DateTimeException khi nhập ngày 32 hay không
        assertThrows(DateTimeException.class, () -> {
            advance.tinhTuoi(32, 1, 2000);
        });
    }
}