package nguyenquyetchien;

import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.CsvSource;
import static org.junit.jupiter.api.Assertions.assertEquals;

public class Test11 {

    @ParameterizedTest(name = "number = {0} -> Kết quả đối xứng mong muốn: {1}")
    @CsvSource({
        "12121, true",   // Số đối xứng nhiều chữ số lẻ (giống ví dụ đề bài)
        "112, false",    // Số không đối xứng (giống ví dụ đề bài)
        "1221, true",    // Số đối xứng số chữ số chẵn
        "7, true",       // Số có 1 chữ số (luôn đối xứng)
        "-121, false"    // Số âm: chuỗi ban đầu là "-121", đảo ngược thành "121-", nên không đối xứng
    })
    void testKiemTraDoiXung(int number, boolean expected) {
        Advance5 advance = new Advance5();
        assertEquals(expected, advance.kiemTraDoiXung(number));
    }
}