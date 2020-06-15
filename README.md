# svcctl

Provide a web api to query/control service in Windows.

## API
### Query
URL: /query/{name}

Method: GET

Headers:
- Authorization: {token}

ContentType: JSON

Response:
- `code`: Status code. If no errors occurred, it should be zero. Otherwise, it's a nonzero value.
- `msg`: Message about calling result.
- `name`: Name of the service.
- `status`: The Service status(bool). It's `true` when the service is running.

Description:
- `name`: Service name.
- `token`: API token for authorization.

### Control
URL: /ctl/{name}/{action}[&timeout={timeout}]

Method: GET

Headers:
- Authorization: {token}

ContentType: JSON

Response:
- `code`: Status code. If no errors occurred, it should be zero. Otherwise, it's a nonzero value.
- `msg`: Message about calling result.
- `name`: Name of the service.
- `action`: The control action of the service.

Description:
- `name`: Service name.
- `action`: Control action(start|stop).
- `token`: API token for authorization.
- `timeout`: Operation timeout(in seconds).

## Server
```
Usage of svcctl.exe:
  -address string
        Address to be serve on. (default ":9001")
  -install
        Install as service.
  -service
        Run as service.
  -token string
        API token for authorization.
  -uninstall
        Uninstall service.
  -version
        Print version info.
```
