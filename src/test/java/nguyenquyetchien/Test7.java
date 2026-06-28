package nguyenquyetchien;

import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.CsvSource;
import static org.junit.jupiter.api.Assertions.assertEquals;

public class Test7 {

    // Test cho hàm Tìm ước số chung lớn nhất
    @ParameterizedTest(name = "USCLN của a={0}, b={1} -> Kết quả mong muốn: {2}")
    @CsvSource({
        "24, 9, 3",   // Trường hợp thông thường: a > bban đầu
        "7, 13, 1",   // Hai số nguyên tố cùng nhau: b > a ban đầu
        "12, 12, 12", // Hai số bằng nhau (vòng lặp while không chạy)
        "50, 25, 25"  // Số này là bội của số kia
    })
    void testUSCLN(int a, int b, int expected) {
        Advance1 advance = new Advance1();
        assertEquals(expected, advance.USCLN(a, b));
    }

    // Test cho hàm Tìm bội số chung nhỏ nhất
    @ParameterizedTest(name = "BSCNN của a={0}, b={1} -> Kết quả mong muốn: {2}")
    @CsvSource({
        "24, 9, 72",
        "7, 13, 91",
        "12, 12, 12",
        "50, 25, 50"
    })
    void testBSCNN(int a, int b, int expected) {
        Advance1 advance = new Advance1();
        assertEquals(expected, advance.BSCNN(a, b));
    }
}