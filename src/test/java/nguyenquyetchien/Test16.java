package nguyenquyetchien;

import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.CsvSource;
import static org.junit.jupiter.api.Assertions.assertEquals;

public class Test16 {

    @ParameterizedTest(name = "Login với username=''{0}'', password=''{1}'' -> Kết quả mong muốn: {2}")
    @CsvSource({
        "user, password, true",   // 1. Đăng nhập thành công (Đúng cả username và password)
        "wrongUser, password, false", // 2. Thất bại do sai username
        "user, wrongPassword, false", // 3. Thất bại do sai password
        "wrongUser, wrongPassword, false", // 4. Thất bại do sai cả hai
        "'', '', false"           // 5. Thất bại khi để trống thông tin
    })
    void testLogin(String username, String password, boolean expectedResult) {
        // Xử lý chuỗi rỗng chuẩn chỉnh phòng trường hợp CsvSource truyền vào dữ liệu trống
        String actualUser = (username == null) ? "" : username;
        String actualPass = (password == null) ? "" : password;

        assertEquals(expectedResult, LoginService.login(actualUser, actualPass));
    }
}