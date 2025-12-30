import { useState, useEffect } from 'react';
import { getEmailRecipients, addEmailRecipient, deleteEmailRecipient } from '../../lib/api';
import './EmailRecipients.css';

const EmailRecipients = () => {
  const [recipients, setRecipients] = useState([]);
  const [loading, setLoading] = useState(true);
  const [showAddForm, setShowAddForm] = useState(false);
  const [newRecipient, setNewRecipient] = useState({
    email: '',
    name: '',
    notification_types: {
      button: true,
      pir: true,
      pir_alert: true,
      voice: true
    }
  });
  const [error, setError] = useState('');
  const [success, setSuccess] = useState('');

  useEffect(() => {
    fetchRecipients();
  }, []);

  const fetchRecipients = async () => {
    try {
      setLoading(true);
      const response = await getEmailRecipients();
      setRecipients(response.data || []);
    } catch (err) {
      setError('Failed to load recipients');
      console.error(err);
    } finally {
      setLoading(false);
    }
  };

  const handleAddRecipient = async (e) => {
    e.preventDefault();
    setError('');
    setSuccess('');

    if (!newRecipient.email || !newRecipient.email.match(/^[^\s@]+@[^\s@]+\.[^\s@]+$/)) {
      setError('Please enter a valid email address');
      return;
    }

    try {
      await addEmailRecipient(newRecipient);
      setSuccess('Recipient added successfully');
      setNewRecipient({
        email: '',
        name: '',
        notification_types: {
          button: true,
          pir: true,
          pir_alert: true,
          voice: true
        }
      });
      setShowAddForm(false);
      fetchRecipients();
      setTimeout(() => setSuccess(''), 3000);
    } catch (err) {
      setError(err.response?.data?.error || 'Failed to add recipient');
    }
  };

  const handleDeleteRecipient = async (id) => {
    if (!confirm('Are you sure you want to delete this recipient?')) return;

    try {
      await deleteEmailRecipient(id);
      setSuccess('Recipient deleted successfully');
      fetchRecipients();
      setTimeout(() => setSuccess(''), 3000);
    } catch (err) {
      setError('Failed to delete recipient');
    }
  };

  const handleNotificationTypeChange = (type) => {
    setNewRecipient(prev => ({
      ...prev,
      notification_types: {
        ...prev.notification_types,
        [type]: !prev.notification_types[type]
      }
    }));
  };

  if (loading) {
    return <div className="email-recipients-loading">Loading recipients...</div>;
  }

  return (
    <div className="email-recipients">
      <div className="email-recipients-header">
        <h3>📧 Email Notifications</h3>
        <button 
          className="btn-add-recipient"
          onClick={() => setShowAddForm(!showAddForm)}
        >
          {showAddForm ? '✕ Cancel' : '+ Add Recipient'}
        </button>
      </div>

      {error && <div className="alert alert-error">{error}</div>}
      {success && <div className="alert alert-success">{success}</div>}

      {showAddForm && (
        <form className="add-recipient-form" onSubmit={handleAddRecipient}>
          <div className="form-group">
            <label>Email Address *</label>
            <input
              type="email"
              value={newRecipient.email}
              onChange={(e) => setNewRecipient({ ...newRecipient, email: e.target.value })}
              placeholder="user@example.com"
              required
            />
          </div>

          <div className="form-group">
            <label>Name (Optional)</label>
            <input
              type="text"
              value={newRecipient.name}
              onChange={(e) => setNewRecipient({ ...newRecipient, name: e.target.value })}
              placeholder="John Doe"
            />
          </div>

          <div className="form-group">
            <label>Notification Types</label>
            <div className="notification-checkboxes">
              <label className="checkbox-label">
                <input
                  type="checkbox"
                  checked={newRecipient.notification_types.button}
                  onChange={() => handleNotificationTypeChange('button')}
                />
                <span>🔔 Button Press</span>
              </label>
              <label className="checkbox-label">
                <input
                  type="checkbox"
                  checked={newRecipient.notification_types.pir}
                  onChange={() => handleNotificationTypeChange('pir')}
                />
                <span>👤 Motion</span>
              </label>
              <label className="checkbox-label">
                <input
                  type="checkbox"
                  checked={newRecipient.notification_types.pir_alert}
                  onChange={() => handleNotificationTypeChange('pir_alert')}
                />
                <span>⚠️ Alert</span>
              </label>
              <label className="checkbox-label">
                <input
                  type="checkbox"
                  checked={newRecipient.notification_types.voice}
                  onChange={() => handleNotificationTypeChange('voice')}
                />
                <span>🎤 Voice</span>
              </label>
            </div>
          </div>

          <button type="submit" className="btn-submit">Add Recipient</button>
        </form>
      )}

      <div className="recipients-list">
        {recipients.length === 0 ? (
          <div className="no-recipients">
            <p>No email recipients configured</p>
            <p className="hint">Add recipients to receive email notifications</p>
          </div>
        ) : (
          <table className="recipients-table">
            <thead>
              <tr>
                <th>Email</th>
                <th>Name</th>
                <th>Notifications</th>
                <th>Status</th>
                <th>Actions</th>
              </tr>
            </thead>
            <tbody>
              {recipients.map((recipient) => (
                <tr key={recipient.id}>
                  <td className="email-cell">{recipient.email}</td>
                  <td>{recipient.name || '-'}</td>
                  <td className="notifications-cell">
                    {recipient.notification_types?.button && <span className="badge">🔔</span>}
                    {recipient.notification_types?.pir && <span className="badge">👤</span>}
                    {recipient.notification_types?.pir_alert && <span className="badge">⚠️</span>}
                    {recipient.notification_types?.voice && <span className="badge">🎤</span>}
                  </td>
                  <td>
                    <span className={`status-badge ${recipient.is_active ? 'active' : 'inactive'}`}>
                      {recipient.is_active ? 'Active' : 'Inactive'}
                    </span>
                  </td>
                  <td>
                    <button
                      className="btn-delete"
                      onClick={() => handleDeleteRecipient(recipient.id)}
                      title="Delete recipient"
                    >
                      🗑️
                    </button>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </div>
    </div>
  );
};

export default EmailRecipients;
